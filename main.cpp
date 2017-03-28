// CameraDeamon
// Last modified by: Selwyn-Lloyd McPherson
// Copyright © 2016-2017 AgriData.

// Include files to use the PYLON API.
#include <pylon/PylonIncludes.h>
#include <pylon/usb/BaslerUsbInstantCamera.h>
#include <pylon/usb/BaslerUsbInstantCameraArray.h>
#include <pylon/usb/_BaslerUsbCameraParams.h>
#include "AgriDataCamera.h"

// Include files to use openCV.
#include "opencv2/core.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"

// GenApi
#include <GenApi/GenApi.h>

// Utilities
#include "zmq.hpp"
#include "AGDUtils.h"

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

// For catching signals
volatile sig_atomic_t sigint_flag = 0;

/**
 * sigint
 *
 * Catches SIGINT to flag a graceful exit. This can be called asynchronously.
 * Actual code execution in the case of SIGINT is handled in main()
 */
void sigint_function(int sig) {
    sigint_flag = 1;
}

/**
 * printIntro
 *
 * An initialization message that prints some relevant information about the
 * program, cameras, etc.
 */
static void printIntro() {
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
    zmq_version(&major, &minor, &patch);

    cout << endl << endl;
    cout << "*------------------------------------------------------*" << endl;
    cout << "***** ZeroMQ Parameters  ******" << endl;
    cout << "*------------------------------------------------------*" << endl;
    cout << "*" << endl;
    cout << "* Current version: " << major << "." << minor << "." << patch << endl;
    cout << "*" << endl;

    string version = AGDUtils::pipe_to_string("git rev-parse HEAD");
    version.pop_back();
    version.pop_back();

    cout << endl << endl;
    cout << "*------------------------------------------------------*" << endl;
    cout << "***** Camera Deamon Parameters  ******" << endl;
    cout << "*------------------------------------------------------*" << endl;
    cout << "*" << endl;
    cout << "* CameraDeamon version: " << version << endl;
    cout << "*" << endl;

    cout << endl << endl;
    cout << "*------------------------------------------------------*" << endl;
    cout << "******* Ready to start acquisition. . . ********" << endl;
    cout << "*------------------------------------------------------*" << endl;
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
    if (tlFactory.EnumerateDevices(devices) == 0) {
		syslog(LOG_ERR, "No camera present!");
        throw RUNTIME_EXCEPTION("No camera present.");
    }
	
	// Initialize the cameras
	AgriDataCamera * cameras[devices.size()];
    for (size_t i = 0; i < devices.size(); ++i) {
		cameras[i] = new AgriDataCamera();
        cameras[i]->Attach(tlFactory.CreateDevice(devices[i]));
        cameras[i]->Initialize();
    }

    // Initialize variables
    char **argv;
    char delimiter = '-';
    int argc = 0;
    int ret;
    int rec;
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
            syslog(LOG_INFO, "SIGINT Caught! Closing gracefully");

            syslog(LOG_INFO, "Destroying ZMQ sockets");
            client.close();
            publisher.close();
			
            syslog(LOG_INFO, "Stopping Cameras");
			for (size_t i = 0; i < devices.size(); ++i)
				cameras[i]->Stop();

            // Wait a second
            usleep(1500000);
            return 0;
        }

        // Non-blocking message handling. If a system call interrupts ZMQ while it is waiting, it will
        // throw an error_t (error_t == 4, errno = EINTR) which would ordinarily cause a crash. Since profilers are constantly
        // interrogating processes, they are interrupted very often. Use the catch to allow things to
        // proceed on smoothly
        try {
            rec = client.recv(&messageR, ZMQ_NOBLOCK);
        } catch (zmq::error_t error) {
            if (errno == EINTR) continue;
        }
        if (rec) {
            received = string(static_cast<char *> (messageR.data()), messageR.size());

            // Parse message
            s = received;
            cout << s << endl;
            tokens = AGDUtils::split(s, delimiter);

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
                        row = AGDUtils::split(tokens[2], '_')[0];
                        direction = AGDUtils::split(tokens[2], '_')[1];

                        logmessage = "Row: " + row + ", Direction: " + direction;
                        syslog(LOG_INFO, logmessage.c_str());
						
						// Generate UUID for scan
						vector <string> fulluuid = AGDUtils::split(AGDUtils::pipe_to_string("cat /proc/sys/kernel/random/uuid"), '-');
						logmessage = "Starting scan " + fulluuid[0];
						syslog(LOG_INFO, logmessage.c_str());
						
						for (size_t i = 0; i < devices.size(); ++i) {
							cameras[i]->scanid = fulluuid[0];
							thread t(&AgriDataCamera::Run, cameras[i]);
							t.detach();
						}
                        reply = id_hash + "_1_RecordingStarted";
                    }
                } else if (tokens[1] == "stop") {
					for (size_t i = 0; i < devices.size(); ++i)
						cameras[i]->Stop();
					reply = id_hash + "_1_CameraStopped";
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
                    for (size_t i = 0; i < devices.size(); ++i) {
						reply += cameras[i]->GetStatus();
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