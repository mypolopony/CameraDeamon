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
//#define DEBUG

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
void sigint(int sig ){
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
            stm << line << '\n';

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
    cout << "*****               OpenCV Parameters             ******" << endl;
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
    cout << "*****               ZeroMQ Parameters             ******" << endl;
    cout << "*------------------------------------------------------*" << endl;
    cout << "*" << endl;
    cout << "* Current version: " << major << "." << minor << "." << patch << endl;
    cout << "*" << endl;

    string version = pipe_to_string("git rev-parse HEAD");
    version.pop_back();
    version.pop_back();

    cout << endl << endl;
    cout << "*------------------------------------------------------*" << endl;
    cout << "*****               Camera Parameters             ******" << endl;
    cout << "*------------------------------------------------------*" << endl;
    cout << "*" << endl;
    cout << "* CameraDeamon version: " << version << endl;
    cout << "*" << endl;

    cout << endl << endl;
    cout << "*------------------------------------------------------*" << endl;
    cout << "*******     Ready to start acquisition. . .     ********" << endl;
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
    string time = pipe_to_string("date --rfc-3339=ns | sed 's/ /T/; s/\(\.......\).*-/\1-/g'");
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
        << "timestamp2,"
        << "Device Seria lNumber,"
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
        << "Gamma,"
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
    string logmessage = "Initializing Camera";
    syslog(LOG_INFO, logmessage.c_str());

    // Variables
    int frames_per_second = 20;
    int exposure_lower_limit = 61;
    int exposure_upper_limit = 1200;

    // Open camera object
    camera.Open();

    // The camera device is parameterized with a default configuration
    // which sets up free-running continuous acquisition.
    camera.StartGrabbing();

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
void writeLatestImage(Mat cv_img, vector<int> compression_params) {

    imwrite("/home/agridata/Desktop/embeddedServer/EmbeddedServer/images/"
            "streaming.png",
            cv_img, compression_params);
}


/**
 * run
 *
 * Main loop
 */
void run(CBaslerUsbInstantCamera &camera) {
    // Configuration / Initialization
    // These heartbeats are in units of images captured
    //    or, in seconds: HEARTBEAT/FRAME_RATE/NUM_CAMS
    int heartbeat_filesize = 200;
    int heartbeat_log = 20;
    int heartbeat = 0;
    int stream_counter = 200;

    // Strings and streams
    ofstream fout;
    ofstream frameout;
    ostringstream oss;
    string timenow;
    string save_path;
    string framefile;
    string logmessage;

    // CPylonImage object as a destination for reformatted image stream
    CPylonImage image;

    // Define 'pixel' output format (to match algorithm optimalization).
    CImageFormatConverter fc;

    // Filestatus for periodically checking filesize
    struct stat filestatus;

    // Generate UUID for scan
    vector <string> fulluuid = split(pipe_to_string("cat /proc/sys/kernel/random/uuid"), '-');
    string runid = fulluuid[0];

    // VideoWriter
    VideoWriter original;

    // Create an instant camera object with the camera device found first.
    // cameraObj camera(CTlFactory::GetInstance().CreateFirstDevice());

    // Print the model name of the camera.
    cout << endl << endl << "Connected Basler USB 3.0 device, type : " << camera.GetDeviceInfo().GetModelName() << endl;

    // Get native width and height from connected camera
    GenApi::CIntegerPtr width(camera.GetNodeMap().GetNode("Width"));
    GenApi::CIntegerPtr height(camera.GetNodeMap().GetNode("Height"));

    // create Mat image template
    Mat cv_img(width->GetValue(), height->GetValue(), CV_8UC3);
    Mat last_img(width->GetValue(), height->GetValue(), CV_8UC3);

    // Time
    timenow = grabTime();

    // Open and write frame logfile headers
    framefile = "/home/agridata/output/" + runid + '_' + timenow + ".txt";
    frameout.open(framefile.c_str(), ios::app);
    writeHeaders(frameout);

    // Open video file
    save_path = "/home/agridata/output/" + runid + '_' + timenow + ".avi";

    cout << Size(width->GetValue(), height->GetValue()) << endl;
    original = VideoWriter(save_path.c_str(), CV_FOURCC('M', 'P', 'E', 'G'), camera.AcquisitionFrameRate.GetValue(),
                           Size(width->GetValue(), height->GetValue()), true);

    // Log
    logmessage = (string) "Opened video file: " + save_path;
    syslog(LOG_INFO, logmessage.c_str());

    // Streaming image compression
    vector<int> compression_params;
    compression_params.push_back(CV_IMWRITE_PNG_COMPRESSION);
    compression_params.push_back(3);

    // if the VideoWriter file is not initialized successfully, exit the
    // program.
    if (!original.isOpened()) {
        logmessage = "Failed to write the video file";
        syslog(LOG_ERR, logmessage.c_str());
    }

    // Set recording to true
    isRecording = true;

    // initiate main loop with algorithm
    while (isRecording) {
        try {
            // Wait for an image and then retrieve it. A timeout of 5000
            // ms is used.
            camera.RetrieveResult(5000, ptrGrabResult, TimeoutHandling_ThrowException);
            fc.OutputPixelFormat = PixelType_BGR8packed;

            // Image grabbed successfully?
            if (ptrGrabResult->GrabSucceeded()) {
                // convert to Mat - openCV format for analysis
                fc.Convert(image, ptrGrabResult);
                cv_img = Mat(ptrGrabResult->GetHeight(), ptrGrabResult->GetWidth(), CV_8UC3,
                             (uint8_t *) image.GetBuffer());

                // write the original stream into file
                original.write(cv_img);

                // write to streaming jpeg
                if (stream_counter == 0) {
                    cv_img.copyTo(last_img);

                    thread t(writeLatestImage, last_img, ref(compression_params));
                    t.detach();
                    stream_counter = 200;
                } else {
                    stream_counter--;
                }

                // Logging frame values
                if (heartbeat % heartbeat_log == 0) {
                    // Update time
                    timenow = grabTime();

                    // Calculate brightness (we can reuse cv_img)
                    //cvtColor(cv_img, cv_img, CV_RGB2HSV);
                    //cv::split(cv_img, channels); // Don't get mixed up with user defined split function!
                    //brightness = mean(channels[2]);
                    //cout << "Camera " <<  "0" << ": " << camera.GetDeviceInfo().GetModelName() << endl;
                    //cout << "GrabSucceeded: " << ptrGrabResult->GrabSucceeded() << endl;
                    //cout << "SizeX: " << ptrGrabResult->GetWidth() << endl;
                    //cout << "SizeY: " << ptrGrabResult->GetHeight() << endl;
                    const uint8_t *pImageBuffer = (uint8_t *) ptrGrabResult->GetBuffer();
                    //cout << "Gray value of first pixel: " << (uint32_t) pImageBuffer[0] << endl;
                    //cout << "Timestamp: " << ptrGrabResult->GetTimeStamp() << endl << endl;

                    // Write log
                    writeFrameLog(frameout, camera, ptrGrabResult->GetTimeStamp());
                }

                if (heartbeat % heartbeat_filesize == 0) {
                    // Floats are required here to prevent int overflow
                    stat(save_path.c_str(), &filestatus);
                    float size = (float) filestatus.st_size;
                    if (size > (float) 3 * (float) 1073741824) { // 1GB = 1073741824 bytes
                        frameout.close();
                        original.release(); // This is done automatically but is included here for clarity

                        timenow = grabTime();
                        save_path = "/home/agridata/output/" + runid + '_' + timenow + ".avi";
                        framefile = "/home/agridata/output/" + runid + '_' + timenow + ".txt";

                        // Open and write logfile headers
                        frameout.open(framefile.c_str(), ios::app);
                        writeHeaders(frameout);

                        original = VideoWriter(save_path.c_str(), CV_FOURCC('M', 'P', 'E', 'G'),
                                               camera.AcquisitionFrameRate.GetValue(),
                                               Size(width->GetValue(), height->GetValue()), true);
                        logmessage = "Opened video file: " + runid + '_' + save_path;
                        syslog(LOG_INFO, logmessage.c_str());
                    }
                }
            }
        }

        catch (const GenICam_3_0_Basler_pylon_v5_0::RuntimeException &e) {
            logmessage = "GenICam Runtime Exception";
            if (camera.IsCameraDeviceRemoved()) {
                logmessage = logmessage + " -- Camera has become disconnected";
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
int stop(CBaslerUsbInstantCamera &camera) {
    logmessage = "Recording Stopped";
    syslog(LOG_INFO, logmessage.c_str());
    isRecording = false;
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

    CBaslerUsbInstantCamera camera(CTlFactory::GetInstance().CreateFirstDevice());
    initializeCamera(camera);

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
          syslog(LOG_INFO, "SIGINT Caught! Closing gracefully".c_str());
          
          // syslog(LOG_INFO, "Destroying ZMQ sockets".c_str());
          // zmq::zmq_close
          // send message to publisher to close?

          syslog(LOG_INFO, "Stopping Cameras".c_str());
          isRecording = false;

          // Wait a second
          usleep(1000000);
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

                // If the camera has become disconnected, don't try anything new
                if (camera.IsCameraDeviceRemoved()) {
                    oss << id_hash << "_0_DeviceIsNotConnected";
                } else {

                    // Choose action
                    if (tokens[1] == "start") {
                        if (isRecording) {
                            oss << id_hash << "_1_AlreadyRecording";
                        } else {
                            // There is a double call to split here, which is better
                            // then initializing a new variable (I think)
                            row = split(tokens[2], '_')[0];
                            direction = split(tokens[2], '_')[1];

                            logmessage = "Row: " + row + ", Direction: " + direction;
                            syslog(LOG_INFO, logmessage.c_str());

                            thread t(run, ref(camera));
                            t.detach();
                            oss << id_hash << "_1_RecordingStarted";
                        }
                    } else if (tokens[1] == "stop") {
                        if (isRecording) {
                            ret = stop(ref(camera));
                            oss << id_hash << "_1_CameraStopped";
                        } else {
                            oss << id_hash << "_1_AlreadyStopped";
                        }
                    } else if (tokens[1] == "BalanceWhiteAuto") {
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
                    } else if (tokens[1] == "GetStatus") {
                        oss << id_hash << "_1_"
                            << "Is recording: " << isRecording << " | "
                            << "Auto FunctionProfile: " << camera.AutoFunctionProfile.GetValue() << " | "
                            << "White Balance Ratio: " << camera.BalanceRatio.GetValue() << " | "
                            << "White Balance Ratio Selector: " << camera.BalanceRatioSelector.GetValue() << " | "
                            << "White Balance Auto: " << camera.BalanceWhiteAuto.GetValue() << " | "
                            << "Exposure Mode: " << camera.ExposureMode.GetValue() << " | "
                            << "Exposure Auto: " << camera.ExposureAuto.GetValue() << " | "
                            << "ExposureTime: " << camera.ExposureTime.GetValue() << " | "
                            << "Exposure Lower Limit: " << camera.AutoExposureTimeLowerLimit.GetValue() << " | "
                            << "Exposure Upper Limit: " << camera.AutoExposureTimeUpperLimit.GetValue() << " | "
                            << "Gain: " << camera.Gain.GetValue() << " | "
                            << "Gain Auto: " << camera.GainAuto.GetValue() << " | "
                            << "Gain Lower Limit: " << camera.AutoGainLowerLimit.GetValue() << " | "
                            << "Gain Upper Limit: " << camera.AutoGainUpperLimit.GetValue() << " | "
                            << "Framerate: " << camera.AcquisitionFrameRate.GetValue() << " | "
                            << "Target Brightness: " << camera.AutoTargetBrightness.GetValue() << " | ";
                    } else {
                        oss << id_hash << "_0_CommandNotFound";
                    }
                }
            } catch (...) {
                oss << id_hash << "_0_ExceptionProcessingCommand";
            }

            // Stringify reply
            reply = oss.str();

            zmq::message_t messageS(reply.size());
            memcpy(messageS.data(), reply.data(), reply.size());
            publisher.send(messageS);

            // Log message and reply (if not GetStatus)
            if (tokens[1] != "GetStatus") {
                logmessage = (string) "MsgRec: " + s + (string) ", ReplySent: " + reply;
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