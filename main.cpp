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
#include "json.hpp"

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
using json = nlohmann::json;


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
    cout << "*------------------------------*" << endl;
    cout << "*****  OpenCV Parameters  ******" << endl;
    cout << "*------------------------------*" << endl;
    cout << "*" << endl;
    cout << "* OpenCV version : " << CV_VERSION << endl;
    cout << "* Major version : " << CV_MAJOR_VERSION << endl;
    cout << "* Minor version : " << CV_MINOR_VERSION << endl;
    cout << "* Subminor version : " << CV_SUBMINOR_VERSION << endl;

    int major, minor, patch;
    zmq_version(&major, &minor, &patch);

    cout << '*' << endl;
    cout << "*------------------------------*" << endl;
    cout << "*****  ZeroMQ Parameters  ******" << endl;
    cout << "*------------------------------*" << endl;
    cout << "*" << endl;
    cout << "* Current version: " << major << "." << minor << "." << patch << endl;

    string version = AGDUtils::pipe_to_string("git rev-parse HEAD");
    version.pop_back();
    version.pop_back();

    cout << '*' << endl;
    cout << "*------------------------------------*" << endl;
    cout << "***** Camera Deamon Parameters  ******" << endl;
    cout << "*------------------------------------*" << endl;
    cout << "*" << endl;
    cout << "* CameraDeamon version: " << version << endl;

    cout << '*' << endl;
    cout << "*--------------------------------------*" << endl;
    cout << "****** Ready to start acquisition ******" << endl;
    cout << "*--------------------------------------*" << endl;
    cout << endl;
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
    printIntro();

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
    int rec;

    string receivedstring;
    json received;
    json reply;
    json status;
    vector <string> tokens;
    zmq::message_t messageR;

    while (true) {
        // Check for and handle signals
        if (sigint_flag) {
            syslog(LOG_INFO, "SIGINT Caught!");

            syslog(LOG_INFO, "Destroying ZMQ sockets");
            client.close();
            publisher.close();

            syslog(LOG_INFO, "Stopping Cameras");
            for (size_t i = 0; i < devices.size(); ++i) {
                cameras[i]->Close();
            }
            
            closelog();

            // Wait a second and a half (I forget why)
            usleep(1500000);
            break;
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

        // Good message
        if (rec) {
            receivedstring = string(static_cast<char *> (messageR.data()), messageR.size());
            received = json::parse(receivedstring);
            cout << "Received: " << receivedstring << endl;

            try {
                reply = {{"id", received["id"]}};

                // Start
                if (received["action"] == "start") {
                    if (isRecording) {
                        reply["status"] = "1";
                        reply["message"] = "Already Recording";
                    } else {
                        logmessage = "Row: " + received["row"].get<string>() + ", Direction: " + received["direction"].get<string>();
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
                        isRecording = true;
                        reply["status"] = "1";
                        reply["message"] = "Recording Started";
                    }
                }
                
                // Stop
                else if (received["action"] == "stop") {
                    if (!isRecording) {
                        reply["status"] = "1";
                        reply["message"] = "Already Stopped";
                    } else {
                        for (size_t i = 0; i < devices.size(); ++i)
                            cameras[i]->Stop();
                        isRecording = false;
                        reply["status"] = "1";
                        reply["message"] = "Recording Stopped";
                    }
                }
                
                // Status
                else if (received["action"] == "status") {
                    for (size_t i = 0; i < devices.size(); ++i) {
                        status = cameras[i]->GetStatus();
                        string sn = status["Serial Number"];
                        reply["message"][sn] = status;
                        reply["status"] = "1";
                    }
                } 
                
                // Snap
                else if (received["action"] == "snap") {
                    for (size_t i = 0; i< devices.size(); ++i) {
                        cameras[i]->Snap();
                    }
                    reply["message"] = "Snapshot Taken";
                    reply["status"] = "1";
                }
                
                // White Balance
                else if (received["action"] == "whitebalance") {
                    for (size_t i = 0; i < devices.size(); ++i) {
                         cameras[i]->BalanceWhiteAuto.SetValue(BalanceWhiteAuto_Once);
                    }
                    reply["status"] = "1";
                    reply["message"] = "White Balance Set";
                }
               
                // Fail
                else {
                    reply["status"] = "0";
                    reply["message"] = "Command Not Found";
                }
            } catch (const GenericException &e) {   
                syslog(LOG_ERR, "An exception occurred.");
                syslog(LOG_ERR, e.GetDescription());

                reply["status"] = "0";
                reply["message"] = "Exception Processing Command";
            }

            zmq::message_t messageS(reply.dump().size());
            memcpy(messageS.data(), reply.dump().c_str(), reply.dump().size());
            publisher.send(messageS);

            //if (received["action"] != "status") {
                cout << "Response: " << reply.dump().c_str() << endl;
            //}
        }
    }
    
    // This code is actually not reachable; a SIGINT will close the daemon gracefully,
    // but the other way to end the service is to turn the computer off, which will
    // probably be the most common end-state. In that case, the cameras never Close(),
    // though I'm not sure what the implications of this are, if any.
    return 0;
}