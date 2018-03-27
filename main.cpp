// CameraDeamon
// Last modified by: Selwyn-Lloyd McPherson
// Copyright © 2016-2017 AgriData.

// Include files to use the PYLON API.
#include <pylon/PylonIncludes.h>
#include <pylon/gige/BaslerGigEInstantCamera.h>
#include <pylon/gige/BaslerGigEInstantCameraArray.h>
#include <pylon/gige/_BaslerGigECameraParams.h>
#include "AgriDataCamera.h"

// Include files to use openCV.
#include "opencv2/core.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"

// GenApi
#include <GenApi/GenApi.h>

// MongoDB & BSON
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>

// Utilities
#include "zmq.hpp"
#include "AGDUtils.h"
#include "json.hpp"

// Logging
#include "easylogging++.h"

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
#include <time.h>
#include <unistd.h>
#include <signal.h>

// uncomment this if debugging
#ifndef DEBUG
#define DEBUG 0 // set debug mode
#endif

// Initialize logging
INITIALIZE_EASYLOGGINGPP

// Namespaces for convenience
using namespace Basler_GigECameraParams;
using namespace Pylon;
using namespace cv;
using namespace GenApi;
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
    LOG(INFO) << "*------------------------------*";
    LOG(INFO) << "*****  OpenCV Parameters  ******";
    LOG(INFO) << "*------------------------------*";
    LOG(INFO) << "*";
    LOG(INFO) << "* OpenCV version : " << CV_VERSION;
    LOG(INFO) << "* Major version : " << CV_MAJOR_VERSION;
    LOG(INFO) << "* Minor version : " << CV_MINOR_VERSION;
    LOG(INFO) << "* Subminor version : " << CV_SUBMINOR_VERSION;

    int major, minor, patch;
    zmq_version(&major, &minor, &patch);

    LOG(INFO) << '*';
    LOG(INFO) << "*------------------------------*";
    LOG(INFO) << "*****  ZeroMQ Parameters  ******";
    LOG(INFO) << "*------------------------------*";
    LOG(INFO) << "*";
    LOG(INFO) << "* Current version: " << major << "." << minor << "." << patch;

    string version = AGDUtils::pipe_to_string("git rev-parse HEAD");
    version.pop_back();
    version.pop_back();

    LOG(INFO) << '*';
    LOG(INFO) << "*------------------------------------*";
    LOG(INFO) << "***** Camera Deamon Parameters  ******";
    LOG(INFO) << "*------------------------------------*";
    LOG(INFO) << "*";
    LOG(INFO) << "* CameraDeamon version: " << version;

    LOG(INFO) << '*';
    LOG(INFO) << "*--------------------------------------*";
    LOG(INFO) << "****** Ready to start acquisition ******";
    LOG(INFO) << "*--------------------------------------*";
    LOG(INFO);
}

/*
 * main
 *
 */
int main() {
    // Configure Logging
    el::Configurations conf("config/easylogging.conf");
    el::Loggers::addFlag(el::LoggingFlag::ColoredTerminalOutput);

    // Register signals
    signal(SIGINT, sigint_function);

    LOG(INFO) << "Camera Deamon has been started";

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

    // Initialize MongoDB connection
    // The use of auto here is unfortunate, but it is apparently recommended
    // The type is actually N8mongocxx7v_noabi10collectionE or something crazy
    mongocxx::instance inst{};
    mongocxx::client conn{mongocxx::uri
        {"mongodb://localhost:27017"}};
    mongocxx::database db = conn["agdb"];
    mongocxx::collection scans = db["scan"];

    // Initialize Pylon (required for any future Pylon fuctions)
    PylonInitialize();
    printIntro();

    // Get the transport layer factory.
    CTlFactory& tlFactory = CTlFactory::GetInstance();

    // Get all attached devices and exit application if no device is found.
    DeviceInfoList_t devices;
    if (tlFactory.EnumerateDevices(devices) == 0) {
        LOG(FATAL) << "No cameras present";
    }

    // Camera Initialization
    AgriDataCamera * cameras[devices.size()];
    try {
        for (size_t i = 0; i < devices.size(); ++i) {
            cameras[i] = new AgriDataCamera();
            cameras[i]->Attach(tlFactory.CreateDevice(devices[i]));
            cameras[i]->Initialize();
        }
    } catch (const GenericException &e) {
        LOG(ERROR) << "Camera Initialization Failed";
        LOG(ERROR) << "Exception caught: " << e.what();
    }

    // Initialize variables
    int rec;
    string receivedstring;
    json received;
    json reply;
    json status;
    string sn;
    vector <string> tokens;
    zmq::message_t messageR;

    while (true) {
        try {
            // Check for and handle signals
            if (sigint_flag) {
                LOG(INFO) << "SIGINT Caught!";
                LOG(INFO) << "Destroying ZMQ sockets";
                client.close();
                publisher.close();

                LOG(INFO) << "Arresting Cameras";
                for (size_t i = 0; i < devices.size(); ++i) {
                    cameras[i]->Close();
                }

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

                try {
                    reply = {
                        {"id", received["id"]}
                    };

                    // Start
                    if (received["action"] == "start") {
                        if (isRecording) {
                            reply["status"] = "1";
                            reply["message"] = "Already Recording";
                        } else {
                            LOG(INFO) << "Row: " + received["row"].get<string>() + ", Direction: " + received["direction"].get<string>();

                            // Generate UUID for scan
                            //vector <string> fulluuid = AGDUtils::split(AGDUtils::pipe_to_string("cat /proc/sys/kernel/random/uuid"), '-');
                            string scanid = AGDUtils::grabTime("%Y-%m-%d_%H-%M");
                            LOG(INFO) << "Starting scan " + scanid;

                            // Generate MongoDB doc
                            auto doc = bsoncxx::builder::basic::document{};
                            
                            doc.append(bsoncxx::builder::basic::kvp("scanid", scanid));
                            doc.append(bsoncxx::builder::basic::kvp("finalized", 0));
                            doc.append(bsoncxx::builder::basic::kvp("problem", 0));
                            doc.append(bsoncxx::builder::basic::kvp("start", bsoncxx::types::b_int64{AGDUtils::grabMilliseconds()}));

                            // Create document *before* running the cameras
                            scans.insert_one(doc.view());

                            for (size_t i = 0; i < devices.size(); ++i) {
                                // Set Scan ID
                                cameras[i]->scanid = scanid;
                                // Set Scan type
                                cameras[i]->calibration = (reply.value("calibration", false) && reply["calibration"].get<bool>());
                            
                                thread t(&AgriDataCamera::Run, cameras[i]);
                                t.detach();
                            }

                            isRecording = true;
                            reply["message"] = "Cameras started";
                            reply["scanid"] = scanid;
                            reply["status"] = "1";
                        }
                    }
                        // Pause
                    else if (received["action"] == "pause") {
                        if (isRecording) {
                            for (size_t i = 0; i < devices.size(); ++i) {
                                sn = cameras[i]->GetDeviceInfo().GetSerialNumber();
                                if (!cameras[i]->isPaused) {
                                    cameras[i]->isPaused = true;
                                    reply["message"][sn] = "Camera paused";
                                } else {
                                    cameras[i]->isPaused = false;
                                    reply["message"][sn] = "Camera unpaused";
                                }
                            }
                            reply["status"] = "1";
                        } else {
                            reply["message"] = "Cameras are not recording!";
                            reply["status"] = "0";
                        }
                    }                        // Stop
                    else if (received["action"] == "stop") {
                        if (!isRecording) {
                            reply["status"] = "1";
                            reply["message"] = "Already Stopped";
                        } else {
                            // Get scanind from the first camera and close out the db entry
                            json status = cameras[0]->GetStatus();
                            string id = status["scanid"];

                            // Using the stream here since it's so popular
                            scans.update_one(bsoncxx::builder::stream::document{}
                            << "scanid" << id << bsoncxx::builder::stream::finalize,
                                    bsoncxx::builder::stream::document{}
                            << "$set" <<
                            bsoncxx::builder::stream::open_document << "end" << bsoncxx::types::b_int64{AGDUtils::grabMilliseconds()}
                            <<
                            bsoncxx::builder::stream::close_document << bsoncxx::builder::stream::finalize);

                            // Stop cameras
                            for (size_t i = 0; i < devices.size(); ++i) {
                                cameras[i]->Stop();
                            }

                            // This sleep (is / may be) necessary to allow the threads to finish
                            // and resources to be released
                            usleep(2500000);

                            for (size_t i = 0; i < devices.size(); ++i) {
                                cameras[i]->Initialize();
                            }
                            isRecording = false;
                            reply["message"] = "Recording Stopped, Cameras Reinitialized";
                            reply["status"] = "1";

                        }
                    }
                        // Status
                    else if (received["action"] == "status") {
                        for (size_t i = 0; i < devices.size(); ++i) {
                            status = cameras[i]->GetStatus();
                            sn = status["Serial Number"];
                            reply["message"][sn] = status;
                        }
                        reply["status"] = "1";
                    }
                        // Snap
                    else if (received["action"] == "snap") {
                        for (size_t i = 0; i < devices.size(); ++i) {
                            cameras[i]->Snap();
                        }
                        reply["message"] = "Snapshot Taken";
                        reply["status"] = "1";
                    }
                        // Auto Function ROI
                    else if (received["action"] == "autoaoi") {
                        for (size_t i = 0; i < devices.size(); ++i) {
                            if (received["camera"].get<std::string>().compare((string) cameras[i]->serialnumber) == 0) {
                                if (received["value"] == 1) {
                                    GenApi::CEnumerationPtr(cameras[i]->GetNodeMap().GetNode("AutoFunctionAOISelector"))->SetIntValue(AutoFunctionAOISelector_AOI1);
                                } else {
                                    GenApi::CEnumerationPtr(cameras[i]->GetNodeMap().GetNode("AutoFunctionAOISelector"))->SetIntValue(AutoFunctionAOISelector_AOI2);
                                }
                            }
                        }
                        reply["status"] = "1";
                        reply["message"] = "Auto Function AOI set for camera " + received["camera"].get<std::string>();
                    }
                        // White Balance
                    else if (received["action"] == "whitebalance") {
                        for (size_t i = 0; i < devices.size(); ++i) {
                            if (received["camera"].get<std::string>().compare(received["camera"].get<std::string>()) == 0) {
                                GenApi::CIntegerPtr(cameras[i]->GetNodeMap().GetNode("BalanceWhiteAuto"))->SetValue(BalanceWhiteAuto_Once);
                            }
                        }
                        reply["status"] = "1";
                        reply["message"] = "White Balance set for camera " + received["camera"].get<std::string>();
                    }
                        // Luminance
                    else if (received["action"] == "luminance") {
                        for (size_t i = 0; i < devices.size(); ++i) {
                            if (received["camera"].get<std::string>().compare((string) cameras[i]->serialnumber) == 0) {
                                GenApi::CIntegerPtr(cameras[i]->GetNodeMap().GetNode("AutoTargetValue"))->SetValue(received["value"].get<int>());
                            }
                        }
                        reply["status"] = "1";
                        reply["message"] = "Target Gray Value changed for " + received["camera"].get<std::string>() + " (" + to_string(received["value"].get<int>()) + ")";
                    }

                } catch (const GenericException &e) {
                    // Error block
                    LOG(ERROR) << "An exception occurred.";
                    LOG(ERROR) << e.GetDescription();
                    reply["status"] = "0";
                    reply["message"] = "Exception Processing Command: " + (string) e.GetDescription();
                }

                // LOG(INFO) << "Sending Response\n";
                zmq::message_t messageS(reply.dump().size());
                memcpy(messageS.data(), reply.dump().c_str(), reply.dump().size());
                publisher.send(messageS);

            }
        } catch (const GenericException &e) {
            LOG(FATAL) << "Exception caught: " << e.GetDescription() << "\n";
        }
    }

    return 0;
}
