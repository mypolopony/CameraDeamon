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

AgriDataCamera::AgriDataCamera() {
}

AgriDataCamera::AgriDataCamera(const AgriDataCamera& orig) {
}

AgriDataCamera::~AgriDataCamera() {
}


/**
 * Initialize
 *
 * Opens the camera and initializes it with some settings
 */
void AgriDataCamera::Initialize() {
    INodeMap& nodemap = this->GetNodeMap();
	CIntegerPtr width;
    CIntegerPtr height;

    // Print the model name of the camera.
    cout << "Initializing device " << this->GetDeviceInfo().GetModelName() << endl;

    // Open camera object ahead of time
    this->Open();

    // Get camera device information.
    cout << "Camera Device Information" << endl
            << "=========================" << endl;
    cout << "Vendor : "
            << CStringPtr(nodemap.GetNode("DeviceVendorName"))->GetValue() << endl;
    cout << "Model : "
            << CStringPtr(nodemap.GetNode("DeviceModelName"))->GetValue() << endl;
    cout << "Firmware version : "
            << CStringPtr(nodemap.GetNode("DeviceFirmwareVersion"))->GetValue() << endl;
    cout << "Serial Number : "
            << CStringPtr(nodemap.GetNode("DeviceSerialNumber"))->GetValue() << endl;
    cout << "Frame Size  : "
            << CIntegerPtr(nodemap.GetNode("Width"))->GetValue() << 'x' << CIntegerPtr(nodemap.GetNode("Height"))->GetValue() << endl << endl;

    // prevent parsing of xml during each StartGrabbing()
    this->StaticChunkNodeMapPoolSize = this->MaxNumBuffer.GetValue();

    // Enable the acquisition frame rate parameter and set the frame rate.
    this->AcquisitionFrameRateEnable.SetValue(true);
    this->AcquisitionFrameRate.SetValue(this->frames_per_second);

    // Exposure time limits
    this->AutoExposureTimeLowerLimit.SetValue(this->exposure_lower_limit);
    this->AutoExposureTimeUpperLimit.SetValue(this->exposure_upper_limit);

    // Minimize Exposure
    this->AutoFunctionProfile.SetValue(AutoFunctionProfile_MinimizeExposureTime);

    // Continuous Auto Gain
    // camera.GainAutoEnable.SetValue(true);
    this->GainAuto.SetValue(GainAuto_Once);
    this->ExposureAuto.SetValue(ExposureAuto_Once);
}

/**
 * Run
 *
 * Main loop
 */
void AgriDataCamera::Run() {
	// ProfilerStart("/tmp/profile.out");
    // Configuration / Initialization
    // These heartbeats are in units of images captured
    // or, in seconds: HEARTBEAT/FRAME_RATE
    int heartbeat_filesize = 200;
    int heartbeat_log = 20;
    int heartbeat = 0;
    int stream_counter = 200;

    // Maximum file size (in GB)
    int max_filesize = 3;

    string output_dir = "/home/agridata/output/";

    // Strings and streams
    ofstream frameout;
    string timenow;
    string save_path;
    string framefile;
    string logmessage;
    string serialnumber;

    // Height / Width
    CIntegerPtr width;
    CIntegerPtr height;

    // CPylonImage object as a destination for reformatted image stream
    CPylonImage image;

	// This smart pointer will receive the grab result data
	CGrabResultPtr ptrGrabResult;

    // Define 'pixel' output format (to match algorithm optimalization).
    CImageFormatConverter fc;
    fc.OutputPixelFormat = PixelType_BGR8packed;

    // Filestatus for periodically checking filesize
    struct stat filestatus;

    output_dir += this->scanid + '/';
    int status = mkdir(output_dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    // Time
    timenow = AGDUtils::grabTime();

    // VideoWriter
    VideoWriter videowriter;
    Mat cv_img;
    Mat last_img;

    // Streaming image compression
    vector<int> compression_params;
    compression_params.push_back(CV_IMWRITE_PNG_COMPRESSION);
    compression_params.push_back(3);

	// Get native width and height from connected camera
	width = this->GetNodeMap().GetNode("Width");
	height = this->GetNodeMap().GetNode("Height");

	// create Mat image template
	cv_img = Mat(width->GetValue(), height->GetValue(), CV_8UC3);
	last_img = Mat(width->GetValue(), height->GetValue(), CV_8UC3);

	// Grab serial number
	serialnumber = this->DeviceSerialNumber.GetValue();

	// Open the log file and write headers
	framefile = output_dir + serialnumber + '_' + timenow + ".txt";
	frameout.open(framefile.c_str());
	writeHeaders(frameout);

	// Open the video file
	save_path = output_dir + serialnumber + '_' + timenow + ".avi";
	videowriter = VideoWriter(save_path.c_str(), CV_FOURCC('M','P','E','G'), this->AcquisitionFrameRate.GetValue(),
			Size(width->GetValue(), height->GetValue()), true);

	// Make sure videowriter was opened successfully
	if (videowriter.isOpened()) {
		logmessage = "Opened video file: " + save_path;
		syslog(LOG_INFO, logmessage.c_str());
	} else {
		logmessage = "Failed to write the video file: " + save_path;
		syslog(LOG_ERR, logmessage.c_str());
	}

    // Set recording to true and start grabbing
    this->isRecording = true;
    this->StartGrabbing();

    // initiate main loop with algorithm
    while (this->isRecording) {
        // Wait for an image and then retrieve it. A timeout of 5000 ms is used.
        this->RetrieveResult(5000, ptrGrabResult, TimeoutHandling_ThrowException);

        try {
            // Image grabbed successfully?
            if (ptrGrabResult->GrabSucceeded()) {
                // convert to Mat (OpenCV) format for analysis
                fc.Convert(image, ptrGrabResult);
                cv_img = Mat(ptrGrabResult->GetHeight(), ptrGrabResult->GetWidth(), CV_8UC3,
                        (uint8_t *) image.GetBuffer());

                // Write the original stream into file
                videowriter << cv_img;

                // Write to streaming image (All Cameras)
                if (stream_counter == 0) {
					cv_img.copyTo(last_img);
					
					thread t(&AgriDataCamera::writeLatestImage, this, ref(last_img), ref(compression_params));
					t.detach();
					stream_counter = 200;
                } else {
                    stream_counter--;
                }

                // Logging frame values
                if (heartbeat % heartbeat_log == 0) {
#if DEBUG
                    // Calculate brightness (we can reuse cv_img)
                    //cvtColor(cv_img, cv_img, CV_RGB2HSV);
                    //cv::split(cv_img, channels); // Don't get mixed up with user defined split function!
                    //brightness = mean(channels[2]);
                    //cout << "Camera " << "0" << ": " << camera.GetDeviceInfo().GetModelName() << endl;
                    //cout << "GrabSucceeded: " << ptrGrabResult->GrabSucceeded() << endl;
                    //cout << "SizeX: " << ptrGrabResult->GetWidth() << endl;
                    //cout << "SizeY: " << ptrGrabResult->GetHeight() << endl;
                    //const uint8_t *pImageBuffer = (uint8_t *) ptrGrabResult->GetBuffer();
                    //cout << "Gray value of first pixel: " << (uint32_t) pImageBuffer[0] << endl;
                    //cout << "Timestamp: " << ptrGrabResult->GetTimeStamp() << endl << endl;
#endif
                    // Write log
                    writeFrameLog(frameout, ptrGrabResult->GetTimeStamp());
                }

                if (heartbeat % heartbeat_filesize == 0) {
                    // Floats are required here to prevent int overflow
                    stat(save_path.c_str(), &filestatus);
                    float size = (float) filestatus.st_size;
                    timenow = AGDUtils::grabTime();

                    if (size > (float) max_filesize * (float) 1073741824) { // 1GB = 1073741824 bytes
						frameout.close();
						videowriter.release(); // This is done automatically but is included here for clarity

						save_path = output_dir + serialnumber + '_' + timenow + ".avi";
						framefile = output_dir + serialnumber + '_' + timenow + ".txt";

						// Open and write logfile headers
						frameout.open(framefile.c_str());
						writeHeaders(frameout);
						
						videowriter = VideoWriter(save_path.c_str(), CV_FOURCC('M', 'P', 'E', 'G'), this->AcquisitionFrameRate.GetValue(), Size(width->GetValue(), height->GetValue()), true);
						logmessage = "Opened video file: " + save_path;
						syslog(LOG_INFO, logmessage.c_str());
                    }
                }
            }

        } catch (const GenICam_3_0_Basler_pylon_v5_0::RuntimeException &e) {
            logmessage = "GenICam Runtime Exception";
                if (this->IsCameraDeviceRemoved()) {
                    logmessage = logmessage + "\nCamera " + this->DeviceSerialNumber.GetValue() + " has become disconnected";
                }
            syslog(LOG_ERR, logmessage.c_str());
            this->isRecording = false;
        } catch (const GenericException &e) {
            logmessage = ptrGrabResult->GetErrorCode() + " " + ptrGrabResult->GetErrorDescription();
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
void AgriDataCamera::writeHeaders(ofstream &fout) {
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
            << "Black Level" << endl;
    fout << oss.str();
}

/**
 * writeFrameLog
 *
 * Output relevant data to the log, one line per frame
 */
void AgriDataCamera::writeFrameLog(ofstream &fout, uint64_t camtime) {
    ostringstream oss;
    oss << this->isRecording << "," << camtime << ',' << this->DeviceSerialNumber.GetValue() << ","
            << this->AutoFunctionProfile.GetValue() << "," << this->BalanceRatio.GetValue() << ","
            << this->BalanceRatioSelector.GetValue() << "," << this->BalanceWhiteAuto.GetValue() << ","
            << this->ExposureMode.GetValue() << "," << this->ExposureAuto.GetValue() << ","
            << this->ExposureTime.GetValue() << "," << this->AutoExposureTimeLowerLimit.GetValue() << ","
            << this->AutoExposureTimeUpperLimit.GetValue() << "," << this->Gain.GetValue() << ","
            << this->GainAuto.GetValue() << "," << this->AutoGainLowerLimit.GetValue() << ","
            << this->AutoGainUpperLimit.GetValue() << "," << this->AcquisitionFrameRate.GetValue() << ","
            << this->AutoTargetBrightness.GetValue() << "," << this->BlackLevel.GetValue() << endl;

    fout << oss.str();
}

/**
 * writeLatestImage
 *
 * If we would like the occasional streaming image to be produced, it can be done here.
 * This is intended to run in a separate thread so as not to block. Compression_params
 * is an OpenCV construct to define the level of compression.
 */
void AgriDataCamera::writeLatestImage(Mat cv_img, vector<int> compression_params) {
	string snumber;
	snumber = this->DeviceSerialNumber.GetValue();
    imwrite("/home/agridata/Desktop/embeddedServer/EmbeddedServer/images/" + snumber + '_' +
            "streaming.png",
            cv_img, compression_params);
}


/**
 * Stop
 *
 * Upon receiving a stop message, set the isRecording flag
 */
void AgriDataCamera::Stop() {
    syslog(LOG_INFO, "Recording Stopped");
    this->isRecording = false;
    this->StopGrabbing();
	//ProfilerStop();
    syslog(LOG_INFO, "*** Done ***");
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
    << "Framerate: " << this->AcquisitionFrameRate.GetValue() << " | "
    << "Target Brightness: " << this->AutoTargetBrightness.GetValue() << " \n ";
    
    return infostream.str();
 }