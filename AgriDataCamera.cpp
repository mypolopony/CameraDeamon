/* 
 * File:   AgriDatacpp
 * Author: agridata
 * 
 * Created on March 13, 2017, 1:33 PM
 */

#include "AgriDataCamera.h"
#include "AGDUtils.h"
#include "json.hpp"

// Include files to use the PYLON API.
#include <pylon/PylonIncludes.h>
#include <pylon/usb/BaslerUsbInstantCamera.h>
#include <pylon/usb/BaslerUsbInstantCameraArray.h>
#include <pylon/usb/_BaslerUsbCameraParams.h>

// GenApi
#include <GenApi/GenApi.h>

// Standard
#include <fstream>
#include <sstream>
#include <thread>

// Other
#include <syslog.h>
#include <sys/stat.h>

// Include files to use openCV.
#include "opencv2/core.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"

// Namespaces
using namespace Basler_UsbCameraParams;
using namespace Pylon;
using namespace GenApi;
using namespace std;
using namespace cv;
using json = nlohmann::json;

// Timers
const int T_FILESIZE = 200;
const int T_LATEST = 300;

/**
 * Constructor
 */
AgriDataCamera::AgriDataCamera() {
}

/**
 * Destructor
 */
AgriDataCamera::~AgriDataCamera() {
}

/**
 * Initialize
 *
 * Opens the camera and initializes it with some settings
 */
void AgriDataCamera::Initialize() {
    PylonAutoInitTerm autoInitTerm;
    
    // Open camera object ahead of time
    // When stopping and restarting the camera, either one must Close() or otherwise
    // check that the camera is not already open
    if (!IsOpen()) {
        Open();
    }
    
    if (!IsGrabbing()) {
        StartGrabbing();
    }

    INodeMap& nodeMap = GetNodeMap();

    // Print the model name of the 
    cout << "Initializing device " << GetDeviceInfo().GetModelName() << endl;
    
    /*
    try {
        string config = "/home/agridata/CameraDeamon/config/" + string(GetDeviceInfo().GetModelName()) + ".pfs";
        cout << "Reading from configuration file: " + config;
        cout << endl;
        CFeaturePersistence::Load(config.c_str(), &nodeMap, true);

    } catch (const GenericException &e) {
        cerr << "An exception occurred." << endl
                << e.GetDescription() << endl;
    }
     */
    
    int frames_per_second = 20;
    int exposure_lower_limit = 52;
    int exposure_upper_limit = 1200;

    // Enable the acquisition frame rate parameter and set the frame rate.
    AcquisitionFrameRateEnable.SetValue(true);
    AcquisitionFrameRate.SetValue(frames_per_second);

    // Exposure time limits
    ExposureAuto.SetValue(ExposureAuto_Continuous);
    AutoExposureTimeLowerLimit.SetValue(exposure_lower_limit);
    AutoExposureTimeUpperLimit.SetValue(exposure_upper_limit);

    // Minimize Exposure
    AutoFunctionProfile.SetValue(AutoFunctionProfile_MinimizeExposureTime);

    // Continuous Auto Gain
    // GainAutoEnable.SetValue(true);
    GainAuto.SetValue(GainAuto_Continuous);
    
    // Number of buffers does not seem to be specified in .pfs file
    // I'm pretty sure the max is 10, so I don't think any other values are valid.
    // Also, this seems to raise trouble when the cameras has already been initialized,
    // i.e. on stop / reinitialization
    try {
        GetStreamGrabberParams().MaxNumBuffer.SetValue(256);
    } catch (...) {
        cerr << "MaxNumBuffer already set" << endl;
    }
   
    // Get Dimensions
    width = Width.GetValue();
    height = Height.GetValue();

    // Print camera device information.
    cout << "Camera Device Information" << endl
            << "=========================" << endl;
    cout << "Vendor : "
            << CStringPtr(GetNodeMap().GetNode("DeviceVendorName"))->GetValue() << endl;
    cout << "Model : "
            << CStringPtr(GetNodeMap().GetNode("DeviceModelName"))->GetValue() << endl;
    cout << "Firmware version : "
            << CStringPtr(GetNodeMap().GetNode("DeviceFirmwareVersion"))->GetValue() << endl;
    cout << "Serial Number : "
            << CStringPtr(GetNodeMap().GetNode("DeviceSerialNumber"))->GetValue() << endl;
    cout << "Frame Size  : "
            << width << 'x' << height << endl << endl;

    // create Mat image template
    cv_img = Mat(width, height, CV_8UC3);
    last_img = Mat(width, height, CV_8UC3);

    // Define pixel output format (to match algorithm optimalization)
    fc.OutputPixelFormat = PixelType_BGR8packed;

    isRecording = false;
    isPaused = false;

    // Streaming image compression
    compression_params.push_back(CV_IMWRITE_PNG_COMPRESSION);
    compression_params.push_back(3);

    // Output parameters
    max_filesize = 3;
    output_prefix = "/home/agridata/output/";
}

/**
 * Run
 *
 * Main loop
 */
void AgriDataCamera::Run() {
    // Strings and streams
    string videofile;
    string framefile;
    string logmessage;
    string time_now;
    string config;
   
    // Filestatus for periodically checking filesize
    struct stat filestatus;
    output_dir = output_prefix + scanid + '/';
    int status = mkdir(output_dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    
    time_now = AGDUtils::grabTime();

    // Set current filename
    save_prefix = output_dir + DeviceSerialNumber() + '_' + time_now;

    // Open the video file
    videofile = save_prefix + ".avi";
    videowriter = VideoWriter(videofile.c_str(), CV_FOURCC('M', 'J', 'P', 'G'), AcquisitionFrameRate.GetValue(),
            Size(width, height), true);

    // Make sure videowriter was opened successfully
    if (videowriter.isOpened()) {
        logmessage = "Opened video file: " + videofile;
        syslog(LOG_INFO, logmessage.c_str());
    } else {
        logmessage = "Failed to write the video file: " + videofile;
        syslog(LOG_ERR, logmessage.c_str());
    }

    framefile =  save_prefix + ".txt";
    frameout.open(framefile);
    if (frameout.is_open()) {
        logmessage = "Opened log file: " + framefile;
        syslog(LOG_INFO, logmessage.c_str());
        writeHeaders();
    } else {
        logmessage = "Failed to open log file: " + framefile;
        syslog(LOG_ERR, logmessage.c_str());
    }

    // Set recording to true and start grabbing
    isRecording = true;
    if (!IsGrabbing()) {
        StartGrabbing();
    }
    
    // Save configuration
    INodeMap& nodeMap = GetNodeMap();
    config = save_prefix + "-config.txt";
    CFeaturePersistence::Save(config.c_str(), &nodeMap);

    // initiate main loop with algorithm
    while (isRecording) {
        if (!isPaused) {
            // Wait for an image and then retrieve it. A timeout of 5000 ms is used.
            RetrieveResult(5000, ptrGrabResult, TimeoutHandling_ThrowException);
            try {
                // Image grabbed successfully?
                if (ptrGrabResult->GrabSucceeded()) {
                    HandleFrame(ptrGrabResult);
                }
            } catch (const GenericException &e) {
                logmessage = ptrGrabResult->GetErrorCode() + "\n" + ptrGrabResult->GetErrorDescription() + "\n" + e.GetDescription();
                syslog(LOG_ERR, logmessage.c_str());
                isRecording = false;
            }
        }
    }
}

/**
 * writeHeaders
 *
 * All good logfiles have headers. These are them
 */
void AgriDataCamera::writeHeaders() {
    ostringstream oss;
    oss << "Timestamp,"
        << "Exposure Time,"
        << "Resulting Frame Rate,"
        << "Current Bandwidth,"
        << "Device Temperature" << endl;

    frameout << oss.str();
}

/**
 * HandleFrame
 *
 * Receive latest frame
 */
void AgriDataCamera::HandleFrame(CGrabResultPtr ptrGrabResult) {
    // convert to Mat (OpenCV) format for analysis
    fc.Convert(image, ptrGrabResult);
    cv_img = Mat(ptrGrabResult->GetHeight(), ptrGrabResult->GetWidth(), CV_8UC3,
            (uint8_t *) image.GetBuffer());

    // Write the original stream into file
    videowriter << cv_img;
    
    // For writing individual files
    // ostringstream time;
    // time << ptrGrabResult->GetTimeStamp();
    
    // Dump compressed JPG
    // imwrite("/home/agridata/output/images/" + time.str() + ".jpg", cv_img);
    
    // Dumping raw TIFF
    // CImagePersistence::Save(ImageFileFormat_Tiff,("/home/agridata/output/raw/" + time.str() + ".tiff").c_str(), ptrGrabResult);
    

    // Write to streaming image
    if (latest_timer == 0) {
        Mat last_img;

        thread t(&AgriDataCamera::writeLatestImage, this, cv_img, ref(compression_params));
        t.detach();
        // writeLatestImage(last_img);
        latest_timer = T_LATEST;
    } else {
        latest_timer--;
    }

    // Check for video rollover
    if (filesize_timer == 0) {
        string logmessage;
        struct stat filestatus;

        string videofile = save_prefix + ".avi";

        // Floats are required here to prevent int overflow
        stat(videofile.c_str(), &filestatus);
        float size = (float) filestatus.st_size;

        if (size > (float) max_filesize * (float) 1073741824) { // 1GB = 1073741824 bytes
            string videofile;
            string framefile;
            string timenow;

            // Close the files
            frameout.close();
            videowriter.release(); // This is done automatically but is included here for clarity

            // New file prefix
            timenow = AGDUtils::grabTime();
            save_prefix = output_dir + DeviceSerialNumber() + '_' + timenow;

            // Open the video writer
            videofile = save_prefix + ".avi";
            videowriter = VideoWriter(videofile.c_str(), CV_FOURCC('M', 'P', 'E', 'G'), AcquisitionFrameRate(), Size(width, height), true);
            if (videowriter.isOpened()) {
                logmessage = "Opened video file: " + videofile;
                syslog(LOG_INFO, logmessage.c_str());
            } else {
                logmessage = "Failed to write the video file: " + videofile;
                syslog(LOG_ERR, logmessage.c_str());
            }

            // Open and write logfile headers
            framefile = save_prefix + ".txt";
            frameout.open(framefile.c_str());
            if (frameout.is_open()) {
                logmessage = "Opened log file: " + framefile;
                syslog(LOG_INFO, logmessage.c_str());
            } else {
                logmessage = "Failed to open log file: " + framefile;
                syslog(LOG_ERR, logmessage.c_str());
            }
            writeHeaders();
        }

        filesize_timer = T_FILESIZE;
    } else {
        filesize_timer--;
    }

    // Write to frame log
    ostringstream oss;
    oss << ptrGrabResult->GetTimeStamp() << ','
            << ExposureTime.GetValue() << ',' 
            << ResultingFrameRate.GetValue() << ','
            << DeviceLinkCurrentThroughput.GetValue() << ','
            << DeviceTemperature.GetValue() << endl;

    frameout << oss.str();

}

/**
 * Snap
 * 
 * Snap will take one photo, in isolation, and save it to the standard steaming
 * image location.
 * 
 * Consider making this json instead of void to return success
 * 
 */

void AgriDataCamera::Snap() {    
    // this !isRecording criterion is enforced because I don't know what the camera's
    // behavior is to ask for one frame while another (continuous) grabbing process is 
    // ongoing, and really I don't think there should be a need for such feature.
    if (!isRecording) {
        CPylonImage image;
        Mat snap_img = Mat(width, height, CV_8UC3);
        CGrabResultPtr ptrGrabResult;

        // There might be a reason to allow the camera to take a few shots first to 
        // allow any auto adjustments to take place.
        uint32_t c_countOfImagesToGrab = 21;
    
        if (!IsGrabbing()) {
            StartGrabbing();
        }

        for (size_t i = 0; i < c_countOfImagesToGrab; ++i) {
            RetrieveResult(5000, ptrGrabResult, TimeoutHandling_ThrowException);
        }
        
        fc.Convert(image, ptrGrabResult);
        snap_img = Mat(ptrGrabResult->GetHeight(), ptrGrabResult->GetWidth(), CV_8UC3,
            (uint8_t *) image.GetBuffer());
        
        thread t(&AgriDataCamera::writeLatestImage, this, snap_img, ref(compression_params));
        t.detach();
    }
}


/**
 * writeLatestImage
 *
 * If we would like the occasional streaming image to be produced, it can be done here.
 * This is intended to run in a separate thread so as not to block. Compression_params
 * is an OpenCV construct to define the level of compression.
 */
void AgriDataCamera::writeLatestImage(Mat img, vector<int> compression_params) {
    string snumber;
    snumber = DeviceSerialNumber.GetValue();
    Mat thumb;
    resize(img, thumb, Size(), 0.2, 0.2);
    
    // Thumbnail
    imwrite("/home/agridata/EmbeddedServer/images/" + snumber + '_' +
            "streaming_t.png",
            thumb, compression_params);
    // Full
    imwrite("/home/agridata/EmbeddedServer/images/" + snumber + '_' +
        "streaming.png",
        img, compression_params);
    
}

/**
 * Stop
 *
 * Upon receiving a stop message, set the isRecording flag
 */
int AgriDataCamera::Stop() {
    syslog(LOG_INFO, "Recording Stopped");
    isRecording = false;
    syslog(LOG_INFO, "*** Done ***");
    frameout.close();
    return 0;
}

/**
 * GetInfo
 *
 * Respond to the heartbeat the data about the camera
 */
json AgriDataCamera::GetStatus() {
    json status;
    status["Serial Number"] = (string) DeviceSerialNumber.GetValue();
    status["Model Name"] = (string) GetDeviceInfo().GetModelName();
    status["Recording"] = isRecording;

    // Something funny here, occasionally the ptrGrabResult is not available
    // even though the camera is grabbing?
    try {
        status["Timestamp"] = ptrGrabResult->GetTimeStamp();
    } catch (...) {
        status["Timestamp"] = "Not Grabbing";
    }
    status["Exposure Time"] = ExposureTime.GetValue();
    status["Resulting Frame Rate"] = ResultingFrameRate.GetValue();
    status["Current Bandwidth"] = DeviceLinkCurrentThroughput.GetValue();
    status["Temperature"] = DeviceTemperature.GetValue();
    try {
        status["ScanID"] = scanid;
    } catch (...) {
        status["ScanID"] = "";
    }
    
    return status;
}