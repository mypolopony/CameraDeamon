// CameraDeamon
// Last modified by: Selwyn-Lloyd McPherson
// Copyright © 2016-2017 AgriData.

// Include files to use the PYLON API.
#include <pylon/PylonIncludes.h>
#include <pylon/usb/BaslerUsbInstantCamera.h>
#include <pylon/usb/BaslerUsbInstantCameraArray.h>
#include <pylon/usb/_BaslerUsbCameraParams.h>

// Include files to use openCV.
#include "opencv2/core.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"

// GenApi
#include <GenApi/GenApi.h>

// Utilities
#include "zmq.hpp"

// Additional include files.
#include <atomic>
#include <ctime>
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

// uncomment this if debugging
#ifndef DEBUG
#define DEBUG 0 // set debug mode
#endif

// Namespaces for convenience
using namespace Basler_UsbCameraParams;
using namespace Pylon;
using namespace GenApi;
using namespace cv;
using namespace std;

// Shared recording boolean
atomic<bool> isRecording(false);

// This smart pointer will receive the grab result data
CGrabResultPtr ptrGrabResult;

// For catching signals
volatile sig_atomic_t sigint_flag = 0;

/**
* sigint
*
* Catches SIGINT to flag a graceful exit. This can be called asynchronously.
* Actual code execution in the case of SIGINT is handled in main()
*/
void sigint_function(int sig ){
    sigint_flag = 1;
}

/**
* pipe_to_string
*
* Grabs the results of a bash command as a string
*/
string pipe_to_string(const char *command) {
    FILE *popen(const char *command, const char *mode);
    int pclose(FILE *stream);
    
    FILE *file = popen(command, "r");
    
    if (file) {
        ostringstream stm;
        
        constexpr size_t MAX_LINE_SZ = 1024;
        char line[MAX_LINE_SZ];
        
        while (fgets(line, MAX_LINE_SZ, file))
        stm << line << 'n';
        
        pclose(file);
        return stm.str();
    }
    
    return "";
}


/**
* printIntro
*
* An initialization message that prints some relevant information about the
* program, cameras, etc.
*/
void printIntro() {
    cout << endl << endl;
    cout << "*------------------------------------------------------*" << endl;
    cout << "***** OpenCV Parameters  ******" << endl;
    cout << "*------------------------------------------------------*" << endl;
    cout << "*" << endl;
    cout << "* OpenCV version : " << CV_VERSION << endl;
    cout << "* Major version : " << CV_MAJOR_VERSION << endl;
    cout << "* Minor version : " << CV_MINOR_VERSION << endl;
    cout << "* Subminor version : " << CV_SUBMINOR_VERSION << endl;
    
    int major, minor, patch;
    zmq_version (&major, &minor, &patch);
    
    cout << endl << endl;
    cout << "*------------------------------------------------------*" << endl;
    cout << "***** ZeroMQ Parameters  ******" << endl;
    cout << "*------------------------------------------------------*" << endl;
    cout << "*" << endl;
    cout << "* Current version: " << major << "." << minor << "." << patch << endl;
    cout << "*" << endl;
    
    string version = pipe_to_string("git rev-parse HEAD");
    version.pop_back();
    version.pop_back();
    
    cout << endl << endl;
    cout << "*------------------------------------------------------*" << endl;
    cout << "***** Camera Parameters  ******" << endl;
    cout << "*------------------------------------------------------*" << endl;
    cout << "*" << endl;
    cout << "* CameraDeamon version: " << version << endl;
    cout << "*" << endl;
    
    cout << endl << endl;
    cout << "*------------------------------------------------------*" << endl;
    cout << "******* Ready to start acquisition. . . ********" << endl;
    cout << "*------------------------------------------------------*" << endl;
}


/**
* split
*
* Splits a string based on delim
*/
vector <string> split(const string &s, char delim) {
    stringstream ss(s);
    string item;
    vector <string> tokens;
    while (getline(ss, item, delim)) {
        tokens.push_back(item);
    }
    return tokens;
}

/**
* grabTime
*
* A call to date (via bash) returns a timestamp
*/
string grabTime() {
    // Many ways to get the string, but this is the easiest
    string time = pipe_to_string("date --rfc-3339=ns | sed 's/ /T/; s/(.......).*-/1-/g'");
    time.pop_back();
    time.pop_back();
    
    // Fix colon issue if necessary
    // replace(time.begin(), time.end(), ':', '_');
    return (time);
}


/**
* writeHeaders
*
* All good logfiles have headers. These are they
*/
void writeHeaders(ofstream &fout) {
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
void writeFrameLog(ofstream &fout, CBaslerUsbInstantCamera &camera, uint64_t camtime) {
    ostringstream oss;
    oss << isRecording << "," << camtime << ',' << camera.DeviceSerialNumber.GetValue() << ","
    << camera.AutoFunctionProfile.GetValue() << "," << camera.BalanceRatio.GetValue() << ","
    << camera.BalanceRatioSelector.GetValue() << "," << camera.BalanceWhiteAuto.GetValue() << ","
    << camera.ExposureMode.GetValue() << "," << camera.ExposureAuto.GetValue() << ","
    << camera.ExposureTime.GetValue() << "," << camera.AutoExposureTimeLowerLimit.GetValue() << ","
    << camera.AutoExposureTimeUpperLimit.GetValue() << "," << camera.Gain.GetValue() << ","
    << camera.GainAuto.GetValue() << "," << camera.AutoGainLowerLimit.GetValue() << ","
    << camera.AutoGainUpperLimit.GetValue() << "," << camera.AcquisitionFrameRate.GetValue() << ","
    << camera.AutoTargetBrightness.GetValue() << "," << camera.BlackLevel.GetValue() << endl;
    
    fout << oss.str();
}


/**
* initializeCamera
*
* Opens the camera and initializes it with some settings
*/
void initializeCamera(CBaslerUsbInstantCamera &camera) {
    INodeMap& nodemap = camera.GetNodeMap();
    GenApi::CIntegerPtr width;
    GenApi::CIntegerPtr height;
    
    // Print the model name of the camera.
    cout << "Initializing device " << camera.GetDeviceInfo().GetModelName() << endl;
    
    // Open camera object
    camera.Open();
    
    // Get camera device information.
    cout << "Camera Device Information" << endl
    << "=========================" << endl;
    cout << "Vendor : "
    << CStringPtr( nodemap.GetNode( "DeviceVendorName") )->GetValue() << endl;
    cout << "Model : "
    << CStringPtr( nodemap.GetNode( "DeviceModelName") )->GetValue() << endl;
    cout << "Firmware version : "
    << CStringPtr( nodemap.GetNode( "DeviceFirmwareVersion") )->GetValue() << endl;
    cout << "Serial Number : "
    << CStringPtr( nodemap.GetNode( "DeviceSerialNumber") )->GetValue() << endl;
    cout << "Frame Size  : "
    << CIntegerPtr( nodemap.GetNode( "Width") )->GetValue() << 'x' << CIntegerPtr( nodemap.GetNode( "Height") )->GetValue() << endl << endl;
    
    // prevent parsing of xml during each StartGrabbing()
    camera.StaticChunkNodeMapPoolSize = camera.MaxNumBuffer.GetValue();
    
    // Variables (TODO: Move to config file)
    int frames_per_second = 20;
    int exposure_lower_limit = 61;
    int exposure_upper_limit = 1200;
    
    // Enable the acquisition frame rate parameter and set the frame rate.
    camera.AcquisitionFrameRateEnable.SetValue(true);
    camera.AcquisitionFrameRate.SetValue(frames_per_second);
    
    // Exposure time limits
    camera.AutoExposureTimeLowerLimit.SetValue(exposure_lower_limit);
    camera.AutoExposureTimeUpperLimit.SetValue(exposure_upper_limit);
    
    // Minimize Exposure
    camera.AutoFunctionProfile.SetValue(AutoFunctionProfile_MinimizeExposureTime);
    
    // Continuous Auto Gain
    // camera.GainAutoEnable.SetValue(true);
    camera.GainAuto.SetValue(GainAuto_Continuous);
    camera.ExposureAuto.SetValue(ExposureAuto_Continuous);
}

/**
* writeLatestImage
*
* If we would like the occasional streaming image to be produced, it can be done here.
* This is intended to run in a separate thread so as not to block. Compression_params
* is an OpenCV construct to define the level of compression.
*/
void writeLatestImage(Mat cv_img, size_t camindex, vector<int> compression_params) {
    stringstream ss;
    ss << camindex;
	
	string camindexstring = ss.str();

    imwrite("/home/agridata/Desktop/embeddedServer/EmbeddedServer/images/" + camindexstring + '_' +
    "streaming.png",
    cv_img, compression_params);
}


/**
* run
*
* Main loop
*/
void run(CBaslerUsbInstantCameraArray &cameras) {
    // Configuration / Initialization
    // These heartbeats are in units of images captured
    // or, in seconds: HEARTBEAT/FRAME_RATE/NUM_CAMS
    int heartbeat_filesize = 200;
    int heartbeat_log = 20;
    int heartbeat = 0;
    int stream_counter = 200;
    
    // Maximum file size (in GB)
    int max_filesize = 3;
    
    static const int num_cams = cameras.GetSize();
    string output_dir = "/home/agridata/output/";
    
    syslog(LOG_INFO, "#!#!# THIS IS A DEBUG LINE #!#!#!");
    
    // Strings and streams
    ofstream frameouts[num_cams];
    string timenow;
    string save_paths[num_cams];
    string framefile;
    string logmessage;
  	string serialnumber;
  	
  	// Height / Width
  	CIntegerPtr width;
  	CIntegerPtr height;
    
    // CPylonImage object as a destination for reformatted image stream
    CPylonImage images[num_cams];
    
    // Define 'pixel' output format (to match algorithm optimalization).
    CImageFormatConverter fc;
    fc.OutputPixelFormat = PixelType_BGR8packed;
    
    // Filestatus for periodically checking filesize
    struct stat filestatus;
    
    // Generate UUID for scan
    vector <string> fulluuid = split(pipe_to_string("cat /proc/sys/kernel/random/uuid"), '-');
	output_dir += fulluuid[0] + '/';
    int status = mkdir(output_dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    
    // Time
    timenow = grabTime();
    
    // VideoWriter
    VideoWriter videowriters[num_cams];
    Mat cv_imgs[num_cams];
    Mat last_imgs[num_cams];
    
    // Streaming image compression
    vector<int> compression_params;
    compression_params.push_back(CV_IMWRITE_PNG_COMPRESSION);
    compression_params.push_back(3);
    
    // Give cameras their own required objects
    for (size_t i = 0; i < num_cams; ++i) {
        // Get native width and height from connected camera
        width = cameras[i].GetNodeMap().GetNode("Width");
        height = cameras[i].GetNodeMap().GetNode("Height");
        
        // create Mat image template
        cv_imgs[i] = Mat(width->GetValue(), height->GetValue(), CV_8UC3);
        last_imgs[i] = Mat(width->GetValue(), height->GetValue(), CV_8UC3);
        
        // Grab serial number
        serialnumber = cameras[i].DeviceSerialNumber.GetValue();
		
        // Open the log file and write headers
        framefile = output_dir + serialnumber + '_' + timenow + ".txt";
        frameouts[i].open(framefile.c_str());
        writeHeaders(frameouts[i]);
        
        // Open the video file
        save_paths[i] = output_dir + serialnumber + '_' + timenow + ".avi";
        videowriters[i] = VideoWriter(save_paths[i].c_str(), CV_FOURCC('M', 'P', 'E', 'G'), cameras[i].AcquisitionFrameRate.GetValue(),
        Size(width->GetValue(), height->GetValue()), true);
        
        // Make sure videowriter was opened successfully
        if (videowriters[i].isOpened()) {
            logmessage = "Opened video file: " + save_paths[i];
            syslog(LOG_INFO, logmessage.c_str());
            } else {
            logmessage = "Failed to write the video file: " + save_paths[i];
            syslog(LOG_ERR, logmessage.c_str());
        }
    }
    
    // Set recording to true and start grabbing
    isRecording = true;
    cameras.StartGrabbing();
    
    // initiate main loop with algorithm
    while (isRecording) {
      // Wait for an image and then retrieve it. A timeout of 5000 ms is used.
      cameras.RetrieveResult(5000, ptrGrabResult, TimeoutHandling_ThrowException);
	  intptr_t camindex;
		
	  try {
		camindex = ptrGrabResult->GetCameraContext();
	  } catch (const GenericException &e) {
			// Error handling
			cerr << "An exception occurred." << endl
			<< e.GetDescription() << endl;
	  }
	  
	  try{
		
            // Image grabbed successfully?
            if (ptrGrabResult->GrabSucceeded()) {
                // convert to Mat (OpenCV) format for analysis
                fc.Convert(images[camindex], ptrGrabResult);
                cv_imgs[camindex] = Mat(ptrGrabResult->GetHeight(), ptrGrabResult->GetWidth(), CV_8UC3,
                (uint8_t *) images[camindex].GetBuffer());
                
                // Write the original stream into file
                videowriters[camindex].write(cv_imgs[camindex]);
                
                // Write to streaming image (All Cameras)
                if (stream_counter == 0) {
                    for ( size_t i = 0; i < num_cams; ++i) {
                        cv_imgs[i].copyTo(last_imgs[i]);
                        
                        thread t(writeLatestImage, last_imgs[i], i, ref(compression_params));
                        t.detach();
                        stream_counter = 200;
                    }
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
                    writeFrameLog(frameouts[camindex], cameras[camindex], ptrGrabResult->GetTimeStamp());
                }
                
                if (heartbeat % heartbeat_filesize == 0) {
                    // Floats are required here to prevent int overflow
                    stat(save_paths[camindex].c_str(), &filestatus);
                    float size = (float) filestatus.st_size;
                    timenow = grabTime();
                    
                    if (size > (float) max_filesize * (float) 1073741824) { // 1GB = 1073741824 bytes
                        
                        for ( size_t i = 0; i < num_cams; ++i) {
                            frameouts[i].close();
                            videowriters[i].release(); // This is done automatically but is included here for clarity
                            
							serialnumber = cameras[i].DeviceSerialNumber.GetValue();
                            save_paths[i] = output_dir + serialnumber + '_' + timenow + ".avi";
                            framefile = output_dir + serialnumber + '_' + timenow + ".txt";
                            
                            // Open and write logfile headers
                            frameouts[i].open(framefile.c_str());
                            writeHeaders(frameouts[i]);
							
							width = cameras[i].GetNodeMap().GetNode("Width");
							height = cameras[i].GetNodeMap().GetNode("Height");
                            
                            videowriters[i] = VideoWriter(save_paths[i].c_str(), CV_FOURCC('M', 'P', 'E', 'G'), cameras[i].AcquisitionFrameRate.GetValue(), Size(width->GetValue(), height->GetValue()), true);
                            logmessage = "Opened video file: " + save_paths[i];
                            syslog(LOG_INFO, logmessage.c_str());
                        }
                    }
                }
            }
        
	  } catch (const GenICam_3_0_Basler_pylon_v5_0::RuntimeException &e) {
            logmessage = "GenICam Runtime Exception";
			for ( size_t i = 0; i < num_cams; ++i) {
				if (cameras[i].IsCameraDeviceRemoved()) {
					logmessage = logmessage + "\nCamera " + cameras[i].DeviceSerialNumber.GetValue() + " has become disconnected";
				}
			}
            syslog(LOG_ERR, logmessage.c_str());
            isRecording = false;
        }
        
        catch (const GenericException &e) {
            logmessage = ptrGrabResult->GetErrorCode() + " " + ptrGrabResult->GetErrorDescription();
            syslog(LOG_ERR, logmessage.c_str());
            isRecording = false;
        }
    }
}


/**
* stop
*
* Upon receiving a stop message, set the isRecording flag
*/
int stop(CBaslerUsbInstantCameraArray& cameras) {
    string logmessage = "Recording Stopped";
    syslog(LOG_INFO, logmessage.c_str());
    isRecording = false;
	cameras.StopGrabbing();
    cout << endl << endl << " *** Done ***" << endl << endl;
    
    return 0;
}

/*
* main
*
*/
int main() {
    // Register signals
    signal(SIGINT, sigint_function);
    
    // Enable logging (to /var/log/agridata.log)
    openlog("CameraDeamon", LOG_CONS | LOG_PID, LOG_LOCAL1);
    string logmessage = "Camera Deamon has been started";
    syslog(LOG_INFO, logmessage.c_str());
    
    // Set Up Timer
    time_t start, future;
    time(&start);
    double seconds;
    
    // Subscribe on port 4999
    zmq::context_t context(1);
    zmq::socket_t client(context, ZMQ_SUB);
    client.connect("tcp://127.0.0.1:4999");
    
    // Publish on 4448
    zmq::socket_t publisher(context, ZMQ_PUB);
    publisher.bind("tcp://*:4998");
    
    client.setsockopt(ZMQ_SUBSCRIBE, "", 0);
    
    // Wait for sockets
    zmq_sleep(1.5);
    
    // Initialize Pylon (required for any future Pylon fuctions)
    PylonInitialize();
    
    // Get the transport layer factory.
    CTlFactory& tlFactory = CTlFactory::GetInstance();
    
    // Get all attached devices and exit application if no device is found.
    DeviceInfoList_t devices;
    if ( tlFactory.EnumerateDevices(devices) == 0 )
    {
        throw RUNTIME_EXCEPTION( "No camera present.");
    }
    
    CBaslerUsbInstantCameraArray cameras(devices.size());
    
    // Initialize the cameras
    for ( size_t i = 0; i < cameras.GetSize(); ++i) {
        cameras[i].Attach( tlFactory.CreateDevice( devices[i]));
        initializeCamera(cameras[i]);
    }
    
    // Initialize variables
    char **argv;
    char delimiter = '-';
    int argc = 0;
    int ret;
    ostringstream oss;
    size_t pos = 0;
    string id_hash;
    string received;
    string reply;
    string s;
    string row = "";
    string direction = "";
    vector <string> tokens;
    
    while (true) {
        // Placeholder for received message
        zmq::message_t messageR;
        
        // Check for signals
        if (sigint_flag) {
            logmessage = "SIGINT Caught! Closing gracefully";
            syslog(LOG_INFO, logmessage.c_str());
            
            logmessage = "Destroying ZMQ sockets";
            syslog(LOG_INFO, logmessage.c_str());
            client.close();
            publisher.close();
            // send message to publisher to close?
            
            logmessage = "Stopping Cameras";
            syslog(LOG_INFO, logmessage.c_str());
            stop(cameras);
            
            // Wait a second
            usleep(1500000);
            return 0;
        }
        
        // Non-blocking message handling
        if (client.recv(&messageR, ZMQ_NOBLOCK)) {
            received = string(static_cast<char *>(messageR.data()), messageR.size());
            
            // Parse message
            s = received;
            cout << s << endl;
            tokens = split(s, delimiter);
            
            try {
                
                id_hash = tokens[0];
				reply = "";
                    
                    // Choose action
                    if (tokens[1] == "start") {
                        if (isRecording) {
                            reply = id_hash + "_1_AlreadyRecording";
                            } else {
                            // There is a double call to split here, which is better
                            // then initializing a new variable (I think)
                            row = split(tokens[2], '_')[0];
                            direction = split(tokens[2], '_')[1];
                            
                            logmessage = "Row: " + row + ", Direction: " + direction;
                            syslog(LOG_INFO, logmessage.c_str());
                            
                            thread t(run, ref(cameras));
                            t.detach();
                            reply = id_hash + "_1_RecordingStarted";
                        }
                        } else if (tokens[1] == "stop") {
                        if (isRecording) {
                            ret = stop(cameras);
                            reply = id_hash + "_1_CameraStopped";
                            } else {
                            reply = id_hash + "_1_AlreadyStopped";
                        }
                        } 
/*
                        else if (tokens[1] == "BalanceWhiteAuto") {
                        if (tokens[2] == "BalanceWhiteAuto_Once") {
                            camera.BalanceWhiteAuto.SetValue(BalanceWhiteAuto_Once);
                            oss << id_hash << "_1_" << tokens[2];
                            } else if (tokens[2] == "BalanceWhiteAuto_Continuous") {
                            camera.BalanceWhiteAuto.SetValue(BalanceWhiteAuto_Continuous);
                            oss << id_hash << "_1_" << tokens[2];
                            } else if (tokens[2] == "BalanceWhiteAuto_Off") {
                            camera.BalanceWhiteAuto.SetValue(BalanceWhiteAuto_Off);
                            oss << id_hash << "_1_" << tokens[2];
                        }
                        } else if (tokens[1] == "AutoFunctionProfile") {
                        if (tokens[2] == "AutoFunctionProfile_MinimizeExposure") {
                            camera.AutoFunctionProfile.SetValue(AutoFunctionProfile_MinimizeExposureTime);
                            oss << id_hash << "_1_" << tokens[2];
                            } else if (tokens[2] == "AutoFunctionProfile_MinimizeGain") {
                            camera.AutoFunctionProfile.SetValue(AutoFunctionProfile_MinimizeGain);
                            oss << id_hash << "_1_" << tokens[2];
                        }
                        } else if (tokens[1] == "GainAuto") {
                        if (tokens[2] == "GainAuto_Once") {
                            camera.GainAuto.SetValue(GainAuto_Once);
                            oss << id_hash << "_1_" << tokens[2];
                            } else if (tokens[2] == "GainAuto_Continuous") {
                            camera.GainAuto.SetValue(GainAuto_Continuous);
                            oss << id_hash << "_1_" << tokens[2];
                            } else if (tokens[2] == "GainAuto_Off") {
                            camera.GainAuto.SetValue(GainAuto_Off);
                            oss << id_hash << "_1_" << tokens[2];
                        }
                        } else if (tokens[1] == "ExposureAuto") {
                        if (tokens[2] == "ExposureAuto_Once") {
                            camera.ExposureAuto.SetValue(ExposureAuto_Once);
                            oss << id_hash << "_1_" << tokens[2];
                            } else if (tokens[2] == "ExposureAuto_Continuous") {
                            camera.ExposureAuto.SetValue(ExposureAuto_Continuous);
                            oss << id_hash << "_1_" << tokens[2];
                            } else if (tokens[2] == "ExposureAuto_Off") {
                            camera.ExposureAuto.SetValue(ExposureAuto_Off);
                            oss << id_hash << "_1_" << tokens[2];
                        }
                        } else if (tokens[1] == "BalanceRatioSelector") {
                        if (tokens[2] == "BalanceRatioSelector_Green") {
                            camera.BalanceRatioSelector.SetValue(BalanceRatioSelector_Green);
                            oss << id_hash << "_1_" << tokens[2];
                            } else if (tokens[2] == "BalanceRatioSelector_Red") {
                            camera.BalanceRatioSelector.SetValue(BalanceRatioSelector_Red);
                            oss << id_hash << "_1_" << tokens[2];
                            } else if (tokens[2] == "BalanceRatioSelector_Blue") {
                            camera.BalanceRatioSelector.SetValue(BalanceRatioSelector_Blue);
                            oss << id_hash << "_1_" << tokens[2];
                        }
                        } else if (tokens[1] == "GainSelector") {
                        camera.GainSelector.SetValue(GainSelector_All);
                        oss << id_hash << "_1_" << tokens[2];
                        } else if (tokens[1] == "Gain") {
                        camera.Gain.SetValue(atof(tokens[2].c_str()));
                        oss << id_hash << "_1_" << tokens[2];
                        } else if (tokens[1] == "BalanceRatio") {
                        camera.BalanceRatio.SetValue(atof(tokens[2].c_str()));
                        oss << id_hash << "_1_" << tokens[2];
                        } else if (tokens[1] == "AutoTargetBrightness") {
                        camera.AutoTargetBrightness.SetValue(atof(tokens[2].c_str()));
                        oss << id_hash << "_1_" << tokens[2];
                        } else if (tokens[1] == "AutoExposureTimeUpperLimit") {
                        camera.AutoExposureTimeUpperLimit.SetValue(atof(tokens[2].c_str()));
                        oss << id_hash << "_1_" << tokens[2];
                        } else if (tokens[1] == "AutoExposureTimeLowerLimit") {
                        camera.AutoExposureTimeLowerLimit.SetValue(atof(tokens[2].c_str()));
                        oss << id_hash << "_1_" << tokens[2];
                        } else if (tokens[1] == "AutoGainUpperLimit") {
                        camera.GainSelector.SetValue(GainSelector_All); // Backup in case we forget
                        camera.AutoGainUpperLimit.SetValue(atof(tokens[2].c_str()));
                        oss << id_hash << "_1_" << tokens[1];
                        } else if (tokens[1] == "AutoGainLowerLimit") { // Backup incase we forget
                        camera.GainSelector.SetValue(GainSelector_All);
                        oss << id_hash << "_1_" << tokens[1];
                        camera.AutoGainLowerLimit.SetValue(atof(tokens[2].c_str()));
                        } else if (tokens[1] == "RowDirection") {
                        oss << id_hash << "_1_" << tokens[1];
                        logmessage = "RowDirection: " + tokens[2];
                        syslog(LOG_INFO, logmessage.c_str());
                        } 
*/
                        else if (tokens[1] == "GetStatus") {
                          reply = id_hash + "_1_";
                          for ( size_t i = 0; i < cameras.GetSize(); ++i) {
                            reply += "Serial Number: " + cameras[i].DeviceSerialNumber.GetValue();
                            oss << "Is recording: " << isRecording << " | "
                            << "Auto FunctionProfile: " << cameras[i].AutoFunctionProfile.GetValue() << " | "
                            << "White Balance Ratio: " << cameras[i].BalanceRatio.GetValue() << " | "
                            << "White Balance Ratio Selector: " << cameras[i].BalanceRatioSelector.GetValue() << " | "
                            << "White Balance Auto: " << cameras[i].BalanceWhiteAuto.GetValue() << " | "
                            << "Exposure Mode: " << cameras[i].ExposureMode.GetValue() << " | "
                            << "Exposure Auto: " << cameras[i].ExposureAuto.GetValue() << " | "
                            << "ExposureTime: " << cameras[i].ExposureTime.GetValue() << " | "
                            << "Exposure Lower Limit: " << cameras[i].AutoExposureTimeLowerLimit.GetValue() << " | "
                            << "Exposure Upper Limit: " << cameras[i].AutoExposureTimeUpperLimit.GetValue() << " | "
                            << "Gain: " << cameras[i].Gain.GetValue() << " | "
                            << "Gain Auto: " << cameras[i].GainAuto.GetValue() << " | "
                            << "Gain Lower Limit: " << cameras[i].AutoGainLowerLimit.GetValue() << " | "
                            << "Gain Upper Limit: " << cameras[i].AutoGainUpperLimit.GetValue() << " | "
                            << "Framerate: " << cameras[i].AcquisitionFrameRate.GetValue() << " | "
                            << "Target Brightness: " << cameras[i].AutoTargetBrightness.GetValue() << " \n ";
                            reply += oss.str();
                          }
                        
                        } else {
                        reply = id_hash + "_0_CommandNotFound";
                    }
                } catch (...) {
                reply = id_hash + "_0_ExceptionProcessingCommand";
            }
            
            zmq::message_t messageS(reply.size());
            memcpy(messageS.data(), reply.data(), reply.size());
            publisher.send(messageS);
            
            // Log message and reply (if not GetStatus)
            if (tokens[1] != "GetStatus") {
                logmessage = (string) "MsgRec: " + s + ", ReplySent: " + reply;
                syslog(LOG_INFO, logmessage.c_str());
            }
            
            // CLear the string buffer
            oss.str("");
        }
        
        // Elapsed time (in seconds)
        seconds = difftime(time(&future), start);
    }
    
    syslog(LOG_INFO, "CameraDeamon is shuttting down gracefully. . .");
    return 0;
}