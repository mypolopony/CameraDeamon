 /*
 * File:   AgriDataCamera.cpp
 * Author: agridata
 */

#include "AgriDataCamera.h"
#include "AGDUtils.h"

// Utilities
#include "zmq.hpp"
#include "zhelpers.hpp"
#include "json.hpp"

// MongoDB & BSON
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>

// Include files to use the PYLON API.
#include <pylon/PylonIncludes.h>
#include <pylon/gige/BaslerGigEInstantCamera.h>
#include <pylon/gige/BaslerGigEInstantCameraArray.h>
#include <pylon/gige/_BaslerGigECameraParams.h>

// GenApi
#include <GenApi/GenApi.h>

// Standard
#include <fstream>
#include <sstream>
#include <thread>
#include <ctime>
#include <iostream>
#include <chrono>
#include <mutex>
#include <ratio>
#include <condition_variable>
#include <ctime>
#include <unistd.h>

// Logging
#include "easylogging++.h"

// System
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

// OpenCV
#include "opencv2/core.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"

// HDF5
#include "H5Cpp.h"

// Definitions
#define REQUEST_TIMEOUT     5000    //  msecs, (> 1000!)
typedef std::chrono::high_resolution_clock Clock;

// Namespaces
using namespace Basler_GigECameraParams;
using namespace Basler_GigEStreamParams;
using namespace Pylon;
using namespace H5;
using namespace std;
using namespace cv;
using namespace GenApi;
using namespace std::chrono;


/**
 * Constructor
 */
AgriDataCamera::AgriDataCamera() :
ctx_(1),
conn{mongocxx::uri
    { MONGODB_HOST}}
{
}


/**
 * Destructor
 */
AgriDataCamera::~AgriDataCamera() {
}


/**
 * s_client_socket
 *
 * Helper function that returns a new configured socket
 * connected to the IMU server
 */
zmq::socket_t * AgriDataCamera::s_client_socket(zmq::context_t & context) {
    LOG(INFO) << "Connecting to IMU server...";
    zmq::socket_t * client = new zmq::socket_t(context, ZMQ_REQ);
    client->connect("tcp://localhost:4997");

    //  Configure socket to not wait at close time
    int linger = 0;
    client->setsockopt(ZMQ_LINGER, &linger, sizeof (linger));
    return client;
}


/**
 * Initialize
 *
 * Opens the camera and initializes it with some settings
 */
void AgriDataCamera::Initialize() {
    // Pylon Initialization
    PylonAutoInitTerm autoInitTerm;
    INodeMap &nodeMap = GetNodeMap();

    // Initialize MongoDB connection
    LOG(INFO) << "INIT : Connecting to MongoDB";
    db = conn["plenty"];
    frames = db["frame"];
    sessions = db["session"];

    // Open camera object ahead of time
    // When stopping and restarting the camera, either one must Close() or otherwise
    // check that the camera is not already open
    if (!IsOpen()) {
        LOG(INFO) << "INIT : Opening Camera";
        Open();
    }

    // Stop grabbing immediately
    LOG(INFO) << "INIT : Stopping Grab";
    StopGrabbing();

    // Start grabbing immediately, though not recording
    if (!IsGrabbing()) {
        LOG(INFO) << "INIT : Setting Output Queue Size to 1";
        OutputQueueSize.SetValue(1);

        // Grab Strategy (can be used to prefer realtime vs. minimizing packet loss)
        LOG(INFO) << "INIT : Setting Grab Strategy to LatestImages";
        StartGrabbing(GrabStrategy_LatestImages);
        //StartGrabbing();    
    }

    // Load config file
    try {
        string config = "/data/CameraDeamon/config/"
                + string(GetDeviceInfo().GetModelName()) + ".pfs";
        LOG(INFO) << "INIT : Reading from configuration file: " + config;
        CFeaturePersistence::Load(config.c_str(), &nodeMap, true);

    } catch (const GenericException &e) {
        LOG(ERROR) << "INIT : An exception occurred: " << e.GetDescription();
    }
    
    // [OPTIONAL] Override Interpacket Delay (GigE only)
    try {
        CIntegerPtr intFeature(nodeMap.GetNode("GevSCPD"));

        if (false) {
            srand(time(NULL));
            intFeature->SetValue((rand() % 12150) + 7150);
            LOG(INFO) << "INIT : Using Randomized Interpacket Delay (" << intFeature->GetValue() << ")";
        } else {
            LOG(INFO) << "INIT : Using Interpacket Delay from Config (" << intFeature->GetValue() << ")";
        }
    } catch (...) {
        LOG(WARNING) << "INIT : Skipping GevSCPD parameter";
    }

    // Get Dimensions
    width = (int) CIntegerPtr(nodeMap.GetNode("Width"))->GetValue();
    height = (int) CIntegerPtr(nodeMap.GetNode("Height"))->GetValue();

    // Identifier
    try { // USB
        serialnumber = (string) CStringPtr(nodeMap.GetNode("DeviceSerialNumber"))->GetValue();
    } catch (...) { // GigE
        serialnumber = (string) CStringPtr(nodeMap.GetNode("DeviceID"))->GetValue();
    }
    modelname = (string) CStringPtr(nodeMap.GetNode("DeviceModelName"))->GetValue();

    // Rotation
    rotation = "ROTATE_0";

    // Color Format
    COLOR_FMT = "RGB";

    // Frame Rate
    HIGH_FPS = AcquisitionFrameRateAbs.GetValue();

    // Processing Mod
    PROCESSING_MOD = 20;
    
    // Print camera device information.
    LOG(INFO) << "Camera Device Information";
    LOG(INFO) << "=========================";
    LOG(INFO) << "Vendor : " << CStringPtr(nodeMap.GetNode("DeviceVendorName"))->GetValue();
    LOG(INFO) << "Model : " << modelname;
    LOG(INFO) << "Firmware version : " << CStringPtr(nodeMap.GetNode("DeviceFirmwareVersion"))->GetValue();
    LOG(INFO) << "Serial Number : " << serialnumber;
    LOG(INFO) << "Frame Size  : " << width << 'x' << height;
    LOG(INFO) << "Total Buffer Count : " << GetStreamGrabberParams().Statistic_Total_Buffer_Count.GetValue();
    LOG(INFO) << "Packet Size : " << GevSCPSPacketSize.GetValue();
    LOG(INFO) << "Inter-packet Delay : " << GevSCPD.GetValue();
    LOG(INFO) << "Bandwidth Assigned : " << GevSCBWA.GetValue();
    LOG(INFO) << "Max Throughput : " << GevSCDMT.GetValue();
    LOG(INFO) << "Target Frame Rate : " << HIGH_FPS;
    LOG(INFO) << "Rotation required : " << rotation;
    LOG(INFO) << "Color format " << COLOR_FMT;
    LOG(INFO) << "Processing every " << PROCESSING_MOD << " frames";

    // Base directory
    save_prefix = "/data/output/plenty/";

    // Create Mat image templates
    cv_img = Mat(width, height, CV_8UC3);
    last_img = Mat(width, height, CV_8UC3);
    resize(last_img, small_last_img, Size(TARGET_HEIGHT, TARGET_WIDTH));

    // Define pixel output format (to match algorithm optimalization)
    fc.OutputPixelFormat = PixelType_BGR8packed;

    // Initial status
    isRecording = false;

    // Timer
    tick = 0;

    // Streaming image compression
    compression_params.push_back(CV_IMWRITE_JPEG_QUALITY);
    compression_params.push_back(30);
    
    // HDF5
    current_hdf5_file = "";
    LOG(INFO) << "Initialization complete!";
}


/**
 * padTo
 *
 * Takes an int and returns string representation, padded
 * to num characters
 */
string AgriDataCamera::padTo(int intval, size_t num) {
    string numstring = to_string(intval);
    if (numstring.length() < num) {
        numstring.insert(0, num - numstring.size(), '0');
    }

    return numstring;
}


/**
 * GetFrameNumber
 *
 * Given a scanid, determine the next frame number. This will keep 
 * frames within a scanid to be monotonically increasing.
 */
int AgriDataCamera::GetFrameNumber(string scanid) {
    // Options
    auto order = bsoncxx::builder::stream::document{} << "frame_number" << -1 << bsoncxx::builder::stream::finalize;
    auto opts = mongocxx::options::find{};
    opts.sort(order.view());

    // Query
    bsoncxx::stdx::optional<bsoncxx::document::value> val = frames.find_one(bsoncxx::builder::stream::document{} 
        << "serialnumber" << serialnumber 
        << "scanid" << scanid 
        << bsoncxx::builder::stream::finalize, opts);

    if (val) {
        LOG(DEBUG) << "Obtaining Frame Number";
        LOG(DEBUG) << bsoncxx::to_json(*val);
        // Increment
        frame_number = nlohmann::json::parse(bsoncxx::to_json(*val))["frame_number"];
        LOG(DEBUG) << bsoncxx::to_json(*val);
        LOG(DEBUG) << "Last number found: " << frame_number;;
        return frame_number++;
    } else {
        // Or start
        return 1;
    }
}

/**
 * saveConfiguration
 *
 * Save camera configuration to designated file
 */
void AgriDataCamera::SaveConfiguration(string outfile) {
    // Wait five seconds for the camera to settle
    sleep(5);

    // Grab config and save
    INodeMap &nodeMap = GetNodeMap();
    CFeaturePersistence::Save(outfile.c_str(), &nodeMap);
}

/**
 * Start
 *
 * Grab one or more images
 */
void AgriDataCamera::Start(nlohmann::json session) {
    // Output parameters
    session_name = session["session_name"];
    bsoncxx::stdx::optional<bsoncxx::document::value> maybe_result = sessions.find_one(bsoncxx::builder::stream::document{}
                                << "session_name" << session_name
                                << bsoncxx::builder::stream::finalize);

    // Create output directory
    string rawdir = save_prefix + session["session_name"] + "/raw/";
    LOG(INFO) << "Making directory: " << rawdir;
    bool success = AGDUtils::mkdirp(rawdir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    // Recording mode
    mode = session["mode"];

    if (mode.compare("avi") == 0) {
        // AVI Output
        string aviout = save_prefix + "raw/" + session_name + ".avi";
        oVideoWriter.open(aviout, CV_FOURCC('H','2','6','4'), 20, Size(TARGET_HEIGHT, TARGET_WIDTH), true);
    } else if (mode.compare("hdf5") == 0) {
        // HDF5 Output
    }

    // Set recording to true and start grabbing
    isRecording = true;

    // Save configuration
    string outfile = rawdir + "config.txt";
    thread t(&AgriDataCamera::SaveConfiguration, this, outfile);
    t.detach();

    // Initialize frame number
    frame_number = 0;

    // Speed
    PROCESSING_MOD = session["speed"];
    
    while (isRecording) {
        try {
            // Wait for an image and then retrieve it
            RetrieveResult(5000, ptrGrabResult, TimeoutHandling_ThrowException);

            // Image grabbed successfully?
            if (ptrGrabResult->GrabSucceeded()) {
                // Create Frame Packet
                FramePacket fp;

                // Computer time
                fp.time_now = AGDUtils::grabMilliseconds();
                last_timestamp = fp.time_now;

                // Image
                fp.img_ptr = ptrGrabResult;

                // Metadata
                fp.session = session;

                // Process the frame
                try {
                    HandleOneFrame(fp);
                } catch (...) {
                    LOG(WARNING) << "Frame slipped!";
                }

                // Oneshot only?
                if (mode.compare("oneshot") == 0) {
                    isRecording = false;
                }

            }
            /* else {
                LOG(ERROR) << "Error: " << ptrGrabResult->GetErrorCode() << " " << ptrGrabResult->GetErrorDescription();
                LOG(WARNING) << serialnumber << " is stressed! Slowing down to " << LOW_FPS;
                try {
                    RT_PROBATION = PROBATION;
                    AcquisitionFrameRateEnable.SetValue(true);
                    AcquisitionFrameRateAbs.SetValue(LOW_FPS);
                    LOG(DEBUG) << "Changed Successfully";
                } catch (const GenericException &e) {
                    LOG(DEBUG) << "Passing on exception: " << e.GetDescription();
                }
            }
            */
        } catch (...) {
            /*
            LOG(ERROR) << ptrGrabResult->GetErrorCode() + "\n"
                    + ptrGrabResult->GetErrorDescription() + "\n"
                    + e.GetDescription();
            isRecording = false;
            */
            LOG(ERROR) << "Exception caught and passed";
        }
    }
    return;
}

/**
 * HandleOneFrame
 *
 * Receive latest frame
 */
void AgriDataCamera::HandleOneFrame(AgriDataCamera::FramePacket fp) {
    double dif;
    struct timeval tp;
    long int start, end;

    clock_t clockstart;
    double duration;

    // Basler time and frame
    ostringstream camera_time;
    camera_time << fp.img_ptr->GetTimeStamp();

    // Convert to BGR8Packed CPylonImage
    clockstart = clock();
    fc.Convert(image, fp.img_ptr);
    duration = 100 * ( clock() - clockstart ) / (double) CLOCKS_PER_SEC;
    // LOG(INFO) << "Native Bayer to BGR8packed: " << duration << "ms";

    // To OpenCV Mat
    clockstart = clock();
    last_img = Mat(fp.img_ptr->GetHeight(), fp.img_ptr->GetWidth(), CV_8UC3, (uint8_t *) image.GetBuffer());
    duration = 100 * ( clock() - clockstart ) / (double) CLOCKS_PER_SEC;
    // LOG(INFO) << "To OpenCV Mat: " << duration << "ms";

    // Resize
    clockstart = clock();
    resize(last_img, small_last_img, Size(TARGET_HEIGHT, TARGET_WIDTH));
    duration = 100 * ( clock() - clockstart ) / (double) CLOCKS_PER_SEC;
    // LOG(INFO) << "Resize: " << duration << "ms";

    // Color
    clockstart = clock();
    cvtColor(small_last_img, small_last_img, CV_BGR2RGB);
    duration = 100 * ( clock() - clockstart ) / (double) CLOCKS_PER_SEC;
    // LOG(INFO) << "Color conversion: " << duration << "ms";

    // Output
    string outname;

    // Encode to JPG Buffer
    clockstart = clock();
    vector<uint8_t> outbuffer;
    static const vector<int> ENCODE_PARAMS = {};
    imencode(".jpg", small_last_img, outbuffer, ENCODE_PARAMS);
    duration = 100 * ( clock() - clockstart ) / (double) CLOCKS_PER_SEC;
    // LOG(INFO) << "JPEG Encode: " << duration << "ms";

    // Output file / time
    vector<string> hms = AGDUtils::split(AGDUtils::grabTime("%H:%M:%S"), ':');

    // Output to either JPG, AVI or HDF5
    if (mode.compare("oneshot") == 0) {
        LOG(INFO) << "Session name: " << fp.session["session_name"];
        string outname = save_prefix + session_name + "/raw/" + session_name + "_" + serialnumber + "_" + hms[0].c_str() + "_" + hms[1].c_str() + "_" + hms[2].c_str() + ".jpg";

        // Write to image
        LOG(INFO) << "About to write: " << outname;
        clockstart = clock();
        imwrite(outname, last_img);
        duration = 100 * ( clock() - clockstart ) / (double) CLOCKS_PER_SEC;
        LOG(INFO) << "Image write: " << duration << "ms";

        // NOTE: By this point, fp.session["session_name"] and session_name are equivalent
        LOG(DEBUG) << "Updating Document";
        string filestub = session_name + "/raw/" + session_name + "_" + serialnumber + "_" + hms[0].c_str() + "_" + hms[1].c_str() + "_" + hms[2].c_str() + ".jpg";
        mongocxx::client _conn{mongocxx::uri{"mongodb://localhost:27017"}};
        mongocxx::database _db = _conn["plenty"];
        mongocxx::collection _task = _db["task"];
        _task.update_one(bsoncxx::builder::stream::document{}
            << "session_name" << session_name << bsoncxx::builder::stream::finalize,
                    bsoncxx::builder::stream::document{}
            << "$set" << bsoncxx::builder::stream::open_document 
                << "files" << bsoncxx::builder::stream::open_array
                    <<  filestub
                    << bsoncxx::builder::stream::close_array << bsoncxx::builder::stream::close_document << bsoncxx::builder::stream::finalize);
    } else if (mode.compare("avi") == 0) {
        // Write to AVI
        clockstart = clock();
        oVideoWriter.write(small_last_img);
        duration = 100 * ( clock() - clockstart ) / (double) CLOCKS_PER_SEC;
        //LOG(INFO) << "Write to AVI: " << duration << "ms";

    } else if (mode.compare("hdf5") == 0) {
        // Write to HDF5
        string basename = session_name + "_" + serialnumber + "_" + hms[0].c_str() + "_" + hms[1].c_str() + ".hdf5";
        string taskname = session_name + "/raw/" + basename;
        string hdf5file = save_prefix + session_name + "/raw/" + basename;
        
        // Should we open a new file?
        if (hdf5file.compare(current_hdf5_file) != 0) {
            LOG(INFO) << "Opening a new file";
            LOG(INFO) << "Current HDF5 file is" << current_hdf5_file;
            LOG(INFO) << "Suggested HDF5 file is" << hdf5file;

            // Close the previous file (if it is a thing)
            if (current_hdf5_file.compare("") != 0) {
                LOG(INFO) << "Closing old HDF5 file";
                H5Fclose(hdf5_out);
                LOG(INFO) << "Creating task";
                AddTask(current_hdf5_file);
            }

            current_hdf5_file = hdf5file;
            hdf5_out = H5Fcreate((current_hdf5_file).c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);

            // Add Metadata
            string VERSION = "3.0.0";
            H5LTset_attribute_string(hdf5_out, "/", "COLOR_FMT", COLOR_FMT.c_str());
            H5LTset_attribute_string(hdf5_out, "/", "VERSION", VERSION.c_str());
            H5LTset_attribute_string(hdf5_out, "/", "ROTATION_NEEDED", rotation.c_str());
        }

        LOG(DEBUG) << "Frame Number: " << frame_number;
        LOG(DEBUG) << "And mod: " << frame_number % PROCESSING_MOD;

        // Save to streaming every FPS
        if (frame_number % 1 == 0) {
            LOG(DEBUG) << "Saving to streaming " << frame_number;
            writeLatestImage(small_last_img, compression_params);
        }
        

        // Create HDF5 Dataset every % PROCESSING_MOD
        if (frame_number % PROCESSING_MOD == 0) {
            LOG(DEBUG) << "Writing new frame " << frame_number;
            clockstart = clock();
            hsize_t buffersize = outbuffer.size();
            try {
                H5LTmake_dataset(hdf5_out, to_string(frame_number).c_str(), 1, &buffersize, H5T_NATIVE_UCHAR, &outbuffer[0]);
            } catch (...) {
                LOG(INFO) << "Frame dropped from HDF5 creation";
            }
        }

        // Increment frame number
        frame_number++;

    }

    /*
    // Dynamic frame rate adjustment
    RT_PROBATION--;
    if (RT_PROBATION == 0) {       // Transition (special case)
          AcquisitionFrameRateEnable.SetValue(true);
          AcquisitionFrameRateAbs.SetValue(HIGH_FPS);  
          LOG(DEBUG) << "Returning to " << HIGH_FPS << " FPS";
    } else if (RT_PROBATION < -1) {         
          RT_PROBATION=-1;        // Back to normal
    }
    */
    return;
}


/**
 * AddTask
 *
 * Create a task entry in the database for file
 */
void AgriDataCamera::AddTask(string targetfile) {
    // New Mongo Connection
    mongocxx::client _conn{mongocxx::uri{"mongodb://localhost:27017"}};
    mongocxx::database _db = _conn["plenty"];
    mongocxx::collection _task = _db["task"];

    // Create the document
    bsoncxx::document::value document = bsoncxx::builder::stream::document{}
            << "session_name" << session_name
            << "cameraid" << serialnumber
            << "mode" << mode
            << "version" << "3.0.0"
            << "timestamap" << AGDUtils::grabMilliseconds()
            << "priority" << 1
            << "files" << bsoncxx::builder::stream::open_array
                <<  targetfile
                << bsoncxx::builder::stream::close_array
            << "progress" << bsoncxx::builder::stream::open_document
                << "health_detection" << 0
                << bsoncxx::builder::stream::close_document
            << bsoncxx::builder::stream::finalize;

    auto ret = _task.insert_one(document.view());
}


float AgriDataCamera::_luminance(cv::Mat input) {
    cv::Mat grayMat;
    cv::cvtColor(input, grayMat, CV_BGR2GRAY);

    // Summation of intensity
    int Totalintensity = 0;
    for (int i = 0; i < grayMat.rows; ++i) {
        for (int j = 0; j < grayMat.cols; ++j) {
            Totalintensity += (int) grayMat.at<uchar>(i, j);
        }
    }

    // Find avg lum of frame
    return Totalintensity / (grayMat.rows * grayMat.cols);
}


/**
 * Luminance
 *
 * Add luminance to an existing db entry (i.e. during scanning)
 */
void AgriDataCamera::Luminance(bsoncxx::oid id, cv::Mat input) {
    // As per http://mongodb.github.io/mongo-cxx-driver/mongocxx-v3/thread-safety/
    // "don't even bother sharing clients. Just give each thread its own"
    mongocxx::client _conn{mongocxx::uri
        { MONGODB_HOST}};
    mongocxx::database _db = _conn["plenty"];
    mongocxx::collection _frames = _db["frame"];

    float avgLum = _luminance(input);

    // Update database entry
    try {
        _frames.update_one(bsoncxx::builder::stream::document{}
        << "_id" << id << bsoncxx::builder::stream::finalize,
                bsoncxx::builder::stream::document{}
        << "$set" << bsoncxx::builder::stream::open_document <<
                "luminance" << avgLum << bsoncxx::builder::stream::close_document << bsoncxx::builder::stream::finalize);
    } catch (exception const &exc) {
        LOG(DEBUG) << "Exception caught " << exc.what() << "\n";
    }
}


/**
 * snapCycle
 *
 * Snap will take one photo, in isolation, and save it to the standard steaming
 * image location.
 *
 * Consider making this json instead of void to return success
 */
void AgriDataCamera::snapCycle() {
    while (true) {
        if (!isRecording) {
            try {
                CPylonImage image;
                Mat snap_img = Mat(width, height, CV_8UC3);
                CGrabResultPtr ptrGrabResult;

                RetrieveResult(5000, ptrGrabResult, TimeoutHandling_ThrowException);
                
                fc.Convert(image, ptrGrabResult);
                snap_img = Mat(ptrGrabResult->GetHeight(), ptrGrabResult->GetWidth(),
                        CV_8UC3, (uint8_t *) image.GetBuffer());

                snap_img.copyTo(last_img);
                writeLatestImage(last_img, compression_params);

            } catch (const GenericException &e) {
                LOG(WARNING) << "Frame slipped from snapCycle:";
                // LOG(WARNING) << ptrGrabResult->GetErrorCode();
                // LOG(WARNING) << ptrGrabResult->GetErrorDescription();
                LOG(WARNING) << e.GetDescription();
            }
        }

        // Sleep for 500ms 
        // This is actually not the output interval, as it depends on
        // timing of the above write (usually ~100ms)
        usleep(400000);
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
    Mat thumb;
    resize(img, thumb, Size(), 0.3, 0.3);

    // Thumbnail
    imwrite("/data/EmbeddedServer/images/" + serialnumber + '_'
            + "streaming_t.jpg", thumb, compression_params);
}


/**
 * Stop
 *
 * Upon receiving a stop message, set the isRecording flag
 */
int AgriDataCamera::Stop() {
    LOG(INFO) << "Recording Stopped";
    isRecording = false;

    if (mode.compare("avi") == 0) {
        // Close AVI
        try {
            LOG(INFO) << "Closing active AVI file";
            oVideoWriter.release();
        } catch(const GenericException &e) {
            LOG(WARNING) << "Closing file failed";
        }
    } else if (mode.compare("hdf5") == 0) {
        // Close HDF5
        try {
            AddTask(current_hdf5_file);
        } catch(const GenericException &e) {
            LOG(WARNING) << "Adding task failed";
        }

        try {
            LOG(INFO) << "Closing active HDF5 file";
            H5Fclose(hdf5_out);
        } catch(const GenericException &e) {
            LOG(WARNING) << "HDF5 file closing failed: " << e.GetDescription();
        }
    }

    LOG(INFO) << "*** Done ***";
    return 0;
}


/**
 * GetStatus
 *
 * Respond to the heartbeat the data about the camera
 */
nlohmann::json AgriDataCamera::GetStatus() {
    nlohmann::json status;
    INodeMap &nodeMap = GetNodeMap();

    status["serial_number"] = serialnumber;
    status["model_name"] = modelname;
    status["recording"] = isRecording;
    status["session_name"] = session_name;

    status["timestamp"] = AGDUtils::grabMilliseconds();

    // Here is the main divergence between GigE and USB Cameras; the nodemap is not standard
    try { // USB
        status["current_gain"] = (float) CFloatPtr(nodeMap.GetNode("Gain"))->GetValue();
        status["exposure_time"] = (float) CFloatPtr(nodeMap.GetNode("ExposureTime"))->GetValue();
        status["resulting_frame_rate"] = (float) CFloatPtr(nodeMap.GetNode("ResultingFrameRate"))->GetValue();
        status["temperature"] = (float) CFloatPtr(nodeMap.GetNode("DeviceTemperature"))->GetValue();
        status["target_brightness"] = (int) CFloatPtr(nodeMap.GetNode("AutoTargetBrightness"))->GetValue();
    } catch (...) { // GigE
        status["current_gain"] = (int) CIntegerPtr(nodeMap.GetNode("GainRaw"))->GetValue(); // Gotcha!
        status["exposure_time"] = (float) CFloatPtr(nodeMap.GetNode("ExposureTimeAbs"))->GetValue();
        BalanceRatioSelector.SetValue(BalanceRatioSelectorEnums::BalanceRatioSelector_Red);
        status["red_balance"] = (float) CFloatPtr(nodeMap.GetNode("BalanceRatioAbs"))->GetValue();
        BalanceRatioSelector.SetValue(BalanceRatioSelectorEnums::BalanceRatioSelector_Blue);
        status["blue_balance"] = (float) CFloatPtr(nodeMap.GetNode("BalanceRatioAbs"))->GetValue();
        BalanceRatioSelector.SetValue(BalanceRatioSelectorEnums::BalanceRatioSelector_Green);
        status["green_balance"] = (float) CFloatPtr(nodeMap.GetNode("BalanceRatioAbs"))->GetValue();
        status["resulting_frame_rate"] = (float) CFloatPtr(nodeMap.GetNode("ResultingFrameRateAbs"))->GetValue();
        status["temperature"] = (float) CFloatPtr(nodeMap.GetNode("TemperatureAbs"))->GetValue();
        status["target_brightness"] = (int) CIntegerPtr(nodeMap.GetNode("AutoTargetValue"))->GetValue();
        status["target_frame_rate"] = AcquisitionFrameRateAbs.GetValue();
        status["probation"] = RT_PROBATION;
    }


    /*
    LOG(DEBUG) << "Serial Number: " << (string) status["serial_number"].get<string>();
    LOG(DEBUG) << "Model Name: " << (string) status["model_name"].get<string>();
    LOG(DEBUG) << "Recording: " << (bool) status["recording"].get<bool>();
    LOG(DEBUG) << "Timestamp: " << (int64_t) status["timestamp"].get<int64_t>();
    LOG(DEBUG) << "Exposure Time: " << (int) status["exposure_time"].get<int>();
    LOG(DEBUG) << "Red Balance: " << status["red_balance"].get<float>();
    LOG(DEBUG) << "Green Balance: " << status["green_balance"].get<float>();
    LOG(DEBUG) << "Blue Balance: " << status["blue_balance"].get<float>();
    LOG(DEBUG) << "Resulting Frame Rate: " << (int) status["resulting_frame_rate"].get<int>();
    LOG(DEBUG) << "Target Frame Rate: " << status["target_frame_rate"].get<float>();
    LOG(DEBUG) << "Current Gain: " << (int) status["current_gain"].get<int>();
    LOG(DEBUG) << "Temperature: " << (int) status["temperature"].get<int>();
    LOG(DEBUG) << "Target Brightness: " << (int) status["target_brightness"].get<int>();
    LOG(DEBUG) << "Probation: " << (int) status["probation"].get<int>();
    */

    bsoncxx::document::value document = bsoncxx::builder::stream::document{}  
            << "Session Name" << session_name
            << "Serial Number" << (string) status["serial_number"].get<string>()
            << "Model Name" << (string) status["model_name"].get<string>()
            << "Recording" << (bool) status["recording"].get<bool>()
            << "Timestamp" << (int64_t) status["timestamp"].get<int64_t>()
            << "Exposure Time" << (int) status["exposure_time"].get<int>()
            << "Red Balance" << status["red_balance"].get<float>()
            << "Green Balance" << status["green_balance"].get<float>()
            << "Blue Balance" << status["blue_balance"].get<float>()
            << "Resulting Frame Rate" << (int) status["resulting_frame_rate"].get<int>()
            << "Target Frame Rate" << status["target_frame_rate"].get<float>()
            << "Current Gain" << (int) status["current_gain"].get<int>()
            << "Temperature" << (int) status["temperature"].get<int>()
            << "Target Brightness" << (int) status["target_brightness"].get<int>()
            << "Probation" << (int) status["probation"].get<int>()
            << bsoncxx::builder::stream::finalize;

    // Insert into the DB
    auto ret = frames.insert_one(document.view());

    /* Debugging
    LOG(DEBUG) << "[" << serialnumber << "] Failed Buffer Count: " << GetStreamGrabberParams().Statistic_Failed_Buffer_Count();
    LOG(DEBUG) << "[" << serialnumber << "] Socket Buffer Size: " << GetStreamGrabberParams().SocketBufferSize();
    LOG(DEBUG) << "[" << serialnumber << "] Buffer Underrun Count: " << GetStreamGrabberParams().Statistic_Buffer_Underrun_Count();
    LOG(DEBUG) << "[" << serialnumber << "] Failed Buffer Count: " << GetStreamGrabberParams().Statistic_Failed_Buffer_Count();
    LOG(DEBUG) << "[" << serialnumber << "] Failed Packet Count: " << GetStreamGrabberParams().Statistic_Failed_Packet_Count();
    LOG(DEBUG) << "[" << serialnumber << "] Total Buffer Count: " << GetStreamGrabberParams().Statistic_Total_Buffer_Count();
    LOG(DEBUG) << "[" << serialnumber << "] Resend Request Count: " << GetStreamGrabberParams().Statistic_Resend_Request_Count();
    LOG(DEBUG) << "[" << serialnumber << "] Resend Packet Count: " << GetStreamGrabberParams().Statistic_Resend_Packet_Count();
    */

    return status;
}
