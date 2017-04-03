/* 
 * File:   AgriDataCamera.cpp
 * Author: agridata
 * 
 * Created on March 13, 2017, 1:33 PM
 */

#include "AgriDataCamera.h"
#include "AGDUtils.h"

// Include files to use the PYLON API.
#include <pylon/PylonIncludes.h>
#include <pylon/usb/BaslerUsbInstantCamera.h>
#include <pylon/usb/BaslerUsbInstantCameraArray.h>
#include <pylon/usb/_BaslerUsbCameraParams.h>

// Profiler
// #include <google/profiler.h>

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

    INodeMap& nodeMap = GetNodeMap();

    // Print the model name of the camera.
    cout << "Initializing device " << GetDeviceInfo().GetModelName() << endl;

    // Open camera object ahead of time
    Open();

    try {
        string config = "/home/agridata/CameraDeamon/config/" + string(GetDeviceInfo().GetModelName()) + ".pfs";
        cout << "Reading from configuration file: " + config;
        cout << endl;
        CFeaturePersistence::Load(config.c_str(), &nodeMap, true);
    } catch (const GenericException &e) {
        cerr << "An exception occurred." << endl
                << e.GetDescription() << endl;
    }

    /*
    frames_per_second = 30;
    exposure_lower_limit = 61;
    exposure_upper_limit = 1200;

    // prevent parsing of xml during each StartGrabbing()
    StaticChunkNodeMapPoolSize = MaxNumBuffer.GetValue();

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
    // camera.GainAutoEnable.SetValue(true);
    GainAuto.SetValue(GainAuto_Once);
     */

    // Get Dimensions
    width = this->Width.GetValue();
    height = this->Height.GetValue();

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

    // Event timers
    filesize_timer = 200;
    latest_timer = 300;

    isRecording = false;

    // Streaming image compression
    compression_params.push_back(CV_IMWRITE_PNG_COMPRESSION);
    compression_params.push_back(3);

    // Output parameters
    max_filesize = 3;
    output_dir = "/home/agridata/output/";
}

/**
 * Run
 *
 * Main loop
 */
void AgriDataCamera::Run() {
    // Strings and streams
    string save_path;
    string framefile;
    string logmessage;

    // Filestatus for periodically checking filesize
    struct stat filestatus;
    output_dir += this->scanid + '/';
    int status = mkdir(output_dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    // Open the video file
    save_path = output_dir + DeviceSerialNumber() + '_' + AGDUtils::grabTime();
    +".avi";
    videowriter = VideoWriter(save_path.c_str(), CV_FOURCC('M', 'P', 'E', 'G'), AcquisitionFrameRate.GetValue(),
            Size(width, height), true);

    // Make sure videowriter was opened successfully
    if (videowriter.isOpened()) {
        logmessage = "Opened video file: " + save_path;
        syslog(LOG_INFO, logmessage.c_str());
    } else {
        logmessage = "Failed to write the video file: " + save_path;
        syslog(LOG_ERR, logmessage.c_str());
    }

    framefile = output_dir + DeviceSerialNumber() + '_' + AGDUtils::grabTime() + ".txt";
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
    StartGrabbing();

    // initiate main loop with algorithm
    while (isRecording) {
        // Wait for an image and then retrieve it. A timeout of 5000 ms is used.
        this->RetrieveResult(5000, ptrGrabResult, TimeoutHandling_ThrowException);

        try {
            // Image grabbed successfully?
            if (ptrGrabResult->GrabSucceeded()) {
                HandleFrame(ptrGrabResult);
            }
        } catch (const GenericException &e) {
            logmessage = ptrGrabResult->GetErrorCode() + "\n" + ptrGrabResult->GetErrorDescription() + "\n" + e.GetDescription();
            syslog(LOG_ERR, logmessage.c_str());
            this->isRecording = false;
        }
    }
}

/**
 * writeHeaders
 *
 * All good logfiles have headers. These are they
 */
void AgriDataCamera::writeHeaders() {
    ostringstream oss;
    oss << "Recording,"
            << "Timestamp,"
            << "Device Serial Number,"
            << "Auto Function Profile,"
            << "White Balance Ratio,"
            << "White Balance Ratio Selector,"
            << "White Balance Auto,"
            << "Exposure Mode,"
            << "Exposure Auto,"
            << "Exposure Time,"
            << "Exposure Lower Limit,"
            << "Exposure Upper Limit,"
            << "Gain,"
            << "Gain Auto,"
            << "Gain Lower Limit,"
            << "Gain Upper Limit,"
            << "Framerate,"
            << "Target Brightness,"
            << "Black Level\n";
    frameout << oss.str();
}

/**
 * writeFrameLog
 *
 * Output relevant data to the log, one line per frame
 */
void AgriDataCamera::HandleFrame(CGrabResultPtr ptrGrabResult) {
    // convert to Mat (OpenCV) format for analysis
    fc.Convert(image, ptrGrabResult);
    cv_img = Mat(ptrGrabResult->GetHeight(), ptrGrabResult->GetWidth(), CV_8UC3,
            (uint8_t *) image.GetBuffer());

    // Write the original stream into file
    videowriter << cv_img;

    // Write to streaming image (All Cameras)
    if (latest_timer == 0) {
        cv_img.copyTo(last_img);

        //thread t(&AgriDataCamera::writeLatestImage, this, ref(last_img), ref(compression_params));
        //t.detach();
        writeLatestImage(last_img);
        latest_timer = 300;
    } else {
        latest_timer--;
    }

    if (filesize_timer == 0) {
        string timenow = AGDUtils::grabTime();
        string save_path = output_dir + DeviceSerialNumber() + '_' + timenow + ".avi";
        struct stat filestatus;

        // Floats are required here to prevent int overflow
        stat(save_path.c_str(), &filestatus);
        float size = (float) filestatus.st_size;
        string logmessage;

        if (size > (float) max_filesize * (float) 1073741824) { // 1GB = 1073741824 bytes
            frameout.close();
            videowriter.release(); // This is done automatically but is included here for clarity

            save_path = output_dir + DeviceSerialNumber() + '_' + timenow + ".avi";
            string framefile = output_dir + DeviceSerialNumber() + '_' + timenow + ".txt";

            // Open and write logfile headers
            frameout.open(framefile.c_str());
            if (frameout.is_open()) {
                logmessage = "Opened log file: " + framefile;
                syslog(LOG_INFO, logmessage.c_str());
            } else {
                logmessage = "Failed to open log file: " + framefile;
                syslog(LOG_ERR, logmessage.c_str());
            }
            writeHeaders();

            videowriter = VideoWriter(save_path.c_str(), CV_FOURCC('M', 'P', 'E', 'G'), AcquisitionFrameRate(), Size(width, height), true);
            if (videowriter.isOpened()) {
                logmessage = "Opened video file: " + save_path;
                syslog(LOG_INFO, logmessage.c_str());
            } else {
                logmessage = "Failed to write the video file: " + save_path;
                syslog(LOG_ERR, logmessage.c_str());
            }
        } else {
            filesize_timer = 200;
        }
    }

    // Write to frame log
    ostringstream oss;
    oss << this->isRecording << "," << ptrGrabResult->GetTimeStamp() << ',' << this->DeviceSerialNumber.GetValue() << ","
            << this->AutoFunctionProfile.GetValue() << "," << this->BalanceRatio.GetValue() << ","
            << this->BalanceRatioSelector.GetValue() << "," << this->BalanceWhiteAuto.GetValue() << ","
            << this->ExposureMode.GetValue() << "," << this->ExposureAuto.GetValue() << ","
            << this->ExposureTime.GetValue() << "," << this->AutoExposureTimeLowerLimit.GetValue() << ","
            << this->AutoExposureTimeUpperLimit.GetValue() << "," << this->Gain.GetValue() << ","
            << this->GainAuto.GetValue() << "," << this->AutoGainLowerLimit.GetValue() << ","
            << this->AutoGainUpperLimit.GetValue() << "," << this->ResultingFrameRate.GetValue() << ","
            << this->AutoTargetBrightness.GetValue() << "," << this->BlackLevel.GetValue() << endl;

    frameout << oss.str();
}

/**
 * writeLatestImage
 *
 * If we would like the occasional streaming image to be produced, it can be done here.
 * This is intended to run in a separate thread so as not to block. Compression_params
 * is an OpenCV construct to define the level of compression.
 */
void AgriDataCamera::writeLatestImage(Mat img) {
    string snumber;
    snumber = this->DeviceSerialNumber.GetValue();
    imwrite("/home/agridata/EmbeddedServer/images/" + snumber + '_' +
            "streaming.png",
            img, compression_params);
}

/**
 * Stop
 *
 * Upon receiving a stop message, set the isRecording flag
 */
void AgriDataCamera::Stop() {
    syslog(LOG_INFO, "Recording Stopped");
    isRecording = false;
    this->Close();
    syslog(LOG_INFO, "*** Done ***");
    frameout.close();
}

/**
 * GetInfo
 *
 * Upon receiving a stop message, set the isRecording flag
 */
string AgriDataCamera::GetStatus() {
    ostringstream infostream;
    infostream << "Serial Number: " << this->DeviceSerialNumber.GetValue()
            << "Is recording: " << this->isRecording << " | "
            << "Auto Function Profile: " << this->AutoFunctionProfile.GetValue() << " | "
            << "White Balance Ratio: " << this->BalanceRatio.GetValue() << " | "
            << "White Balance Ratio Selector: " << this->BalanceRatioSelector.GetValue() << " | "
            << "White Balance Auto: " << this->BalanceWhiteAuto.GetValue() << " | "
            << "Exposure Mode: " << this->ExposureMode.GetValue() << " | "
            << "Exposure Auto: " << this->ExposureAuto.GetValue() << " | "
            << "ExposureTime: " << this->ExposureTime.GetValue() << " | "
            << "Exposure Lower Limit: " << this->AutoExposureTimeLowerLimit.GetValue() << " | "
            << "Exposure Upper Limit: " << this->AutoExposureTimeUpperLimit.GetValue() << " | "
            << "Gain: " << this->Gain.GetValue() << " | "
            << "Gain Auto: " << this->GainAuto.GetValue() << " | "
            << "Gain Lower Limit: " << this->AutoGainLowerLimit.GetValue() << " | "
            << "Gain Upper Limit: " << this->AutoGainUpperLimit.GetValue() << " | "
            << "Framerate: " << this->ResultingFrameRate.GetValue() << " | "
            << "Target Brightness: " << this->AutoTargetBrightness.GetValue() << " \n ";

    return infostream.str();
}