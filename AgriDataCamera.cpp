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
    LOG(INFO) << "Connecting to MongoDB";
    db = conn["plenty"];
    frames = db["frame"];

    // Open camera object ahead of time
    // When stopping and restarting the camera, either one must Close() or otherwise
    // check that the camera is not already open
    if (!IsOpen()) {
        Open();
    }

    // Start grabbing immediately, though not recording
    if (!IsGrabbing()) {
        StartGrabbing();
    }

    // Print the camera identity
    LOG(INFO) << "Initializing device " << GetDeviceInfo().GetModelName();

    // Load config file
    try {
        string config = "/data/CameraDeamon/config/"
                + string(GetDeviceInfo().GetModelName()) + ".pfs";
        LOG(INFO) << "Reading from configuration file: " + config;
        CFeaturePersistence::Load(config.c_str(), &nodeMap, true);

    } catch (const GenericException &e) {
        LOG(ERROR) << "An exception occurred: " << e.GetDescription();
    }
    
    // Turn the Test Image off
    // TestImageSelector.SetValue( TestImageSelector_Off );

    // Grab Strategy (can be used to prefer realtime vs. minimizing packet loss)
    CIntegerPtr strategy(nodeMap.GetNode("GrabStrategy"));
    strategy->SetValue(GrabStrategy_LatestImageOnly);

    // [OPTIONAL] Override Interpacket Delay (GigE only)
    try {
        CIntegerPtr intFeature(nodeMap.GetNode("GevSCPD"));

        if (false) {
            srand(time(NULL));
            intFeature->SetValue((rand() % 12150) + 7150);
            LOG(INFO) << "Using Randomized Interpacket Delay (" << intFeature->GetValue() << ")";
        } else {
            LOG(INFO) << "Using Interpacket Delay from Config (" << intFeature->GetValue() << ")";
        }
    } catch (...) {
        LOG(WARNING) << "Skipping GevSCPD parameter";
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
    
    // Print camera device information.
    LOG(INFO) << "Camera Device Information";
    LOG(INFO) << "=========================";
    LOG(INFO) << "Vendor : " << CStringPtr(nodeMap.GetNode("DeviceVendorName"))->GetValue();
    LOG(INFO) << "Model : " << modelname;
    LOG(INFO) << "Firmware version : " << CStringPtr(nodeMap.GetNode("DeviceFirmwareVersion"))->GetValue();
    LOG(INFO) << "Serial Number : " << serialnumber;
    LOG(INFO) << "Frame Size  : " << width << 'x' << height;
    LOG(INFO) << "Max Buffer Size : " << GetStreamGrabberParams().Statistic_Total_Buffer_Count.GetValue();
    LOG(INFO) << "Packet Size : " << GevSCPSPacketSize.GetValue();
    LOG(INFO) << "Inter-packet Delay : " << GevSCPD.GetValue();
    LOG(INFO) << "Bandwidth Assigned : " << GevSCBWA.GetValue();
    LOG(INFO) << "Max Throughput : " << GevSCDMT.GetValue();
    LOG(INFO) << "Target Frame Rate : " << HIGH_FPS;
    LOG(INFO) << "Rotation required : " << rotation;
    LOG(INFO) << "Color format " << COLOR_FMT;

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
void AgriDataCamera::Start(nlohmann::json task) {
    // Output parameters
    string session_name = task["session_name"];
    save_prefix = "/data/output/plenty/" + task["session_name"] + "/raw/";
    bool success = AGDUtils::mkdirp(save_prefix.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    string mode = task["mode"];

    if (mode.compare("avi") == 0) {
        // AVI Output
        string aviout = save_prefix + task["session_name"] + ".avi";
        oVideoWriter.open(aviout, CV_FOURCC('H','2','6','4'), 20, Size(TARGET_HEIGHT, TARGET_WIDTH), true);
    } else if (mode.compare("hdf5") == 0) {
        // HDF5 Output
        hdf5_out = H5Fcreate((save_prefix + task["session_name"] + ".hdf5").c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
        string VERSION = "3.0.0";
        H5LTset_attribute_string(hdf5_out, "/", "COLOR_FMT", COLOR_FMT.c_str());
        H5LTset_attribute_string(hdf5_out, "/", "VERSION", VERSION.c_str());
        H5LTset_attribute_string(hdf5_out, "/", "ROTATION_NEEDED", rotation.c_str());
    }

    // Set recording to true and start grabbing
    isRecording = true;

    // Save configuration
    string outfile = save_prefix + "config.txt";
    thread t(&AgriDataCamera::SaveConfiguration, this, outfile);
    t.detach();

    // Initialize frame number
    frame_number = 0;
    
    while (isRecording) {
        try {
        // Wait for an image and then retrieve it. A timeout of 5000 ms is used.
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
                fp.task = task;

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

            } else {
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
        } catch (const GenericException &e) {
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

    // Grab the task
    clockstart = clock();
    mongocxx::collection task = db["task"];
    string taskid = fp.task["_id"]["$oid"];
    duration = 100 * ( clock() - clockstart ) / (double) CLOCKS_PER_SEC;
    //LOG(INFO) << "Task grab: " << duration << "ms";

    // Basler time and frame
    ostringstream camera_time;
    camera_time << fp.img_ptr->GetTimeStamp();

    // Convert to BGR8Packed CPylonImage
    clockstart = clock();
    fc.Convert(image, fp.img_ptr);
    duration = 100 * ( clock() - clockstart ) / (double) CLOCKS_PER_SEC;
    //LOG(INFO) << "Native Bayer to BGR8packed: " << duration << "ms";

    // To OpenCV Mat
    clockstart = clock();
    last_img = Mat(fp.img_ptr->GetHeight(), fp.img_ptr->GetWidth(), CV_8UC3, (uint8_t *) image.GetBuffer());
    duration = 100 * ( clock() - clockstart ) / (double) CLOCKS_PER_SEC;
    //LOG(INFO) << "To OpenCV Mat: " << duration << "ms";

    // Resize
    clockstart = clock();
    resize(last_img, small_last_img, Size(TARGET_HEIGHT, TARGET_WIDTH));
    duration = 100 * ( clock() - clockstart ) / (double) CLOCKS_PER_SEC;
    //LOG(INFO) << "Resize: " << duration << "ms";

    // Color
    clockstart = clock();
    cvtColor(small_last_img, small_last_img, CV_BGR2RGB);
    duration = 100 * ( clock() - clockstart ) / (double) CLOCKS_PER_SEC;
    //LOG(INFO) << "Color conversion: " << duration << "ms";

    // Output
    string outname;

    // Encode to JPG Buffer
    clockstart = clock();
    vector<uint8_t> outbuffer;
    static const vector<int> ENCODE_PARAMS = {};
    imencode(".jpg", small_last_img, outbuffer, ENCODE_PARAMS);
    duration = 100 * ( clock() - clockstart ) / (double) CLOCKS_PER_SEC;
    //LOG(INFO) << "JPEG Encode: " << duration << "ms";

    // Output to either JPG or AVI
    string mode = fp.task["mode"];
    if (mode.compare("oneshot") == 0) {
        // Write to image
        clockstart = clock();
        //LOG(INFO) << "FILE:";
        //LOG(INFO) << fp.task["_files"][0];
        outname = fp.task["_files"][0];
        //LOG(INFO) << "About to write: " << outname;
        imwrite(outname, last_img);
        duration = 100 * ( clock() - clockstart ) / (double) CLOCKS_PER_SEC;
        //LOG(INFO) << "Image write: " << duration << "ms";

        //LOG(DEBUG) << "Creating Document";
        bsoncxx::builder::basic::document camerainfo{};
        camerainfo.append(bsoncxx::builder::basic::kvp("image", outname));

        //LOG(DEBUG) << "Updating";
        task.find_one(bsoncxx::builder::stream::document{} << "_id"
                        << taskid 
                        << bsoncxx::builder::stream::finalize);

        task.update_one(
                bsoncxx::builder::basic::make_document(bsoncxx::builder::basic::kvp("_id", taskid)),
                bsoncxx::builder::basic::make_document(bsoncxx::builder::basic::kvp("$set", 
                    bsoncxx::builder::basic::make_document(bsoncxx::builder::basic::kvp("image", outname)))));
        //LOG(DEBUG) << "Done updating";

    } else if (mode.compare("avi") == 0) {
        // Write to AVI
        clockstart = clock();
        oVideoWriter.write(small_last_img);
        duration = 100 * ( clock() - clockstart ) / (double) CLOCKS_PER_SEC;
        //LOG(INFO) << "Write to AVI: " << duration << "ms";
    } else if (mode.compare("hdf5") == 0) {
        /*
        // Write to HDF5
        vector<string> hms = AGDUtils::split(AGDUtils::grabTime("%H:%M:%S"), ':');
        string hdf5file = fp.task['session_name'] + "_" + serialnumber + "_" + hms[0].c_str() + "_" + hms[1].c_str() + "_" + COLOR_FMT + ".hdf5";

        // Should we open a new file?
        if (hdf5file.compare(current_hdf5_file) != 0) {

            // Close the previous file (if it is a thing)
            if (current_hdf5_file.compare("") != 0) {
                H5Fclose(hdf5_out);
                AddTask(current_hdf5_file);
            }
            
            current_hdf5_file = hdf5file;
            LOG(INFO) << "HDF5 File: " << save_prefix + current_hdf5_file;
            hdf5_out = H5Fcreate((save_prefix + current_hdf5_file).c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);

            // Add Metadata
            string VERSION = "3.0.0";
            H5LTset_attribute_string(hdf5_out, "/", "COLOR_FMT", COLOR_FMT.c_str());
            H5LTset_attribute_string(hdf5_out, "/", "VERSION", VERSION.c_str());
            H5LTset_attribute_string(hdf5_out, "/", "ROTATION_NEEDED", rotation.c_str());
        }
        */

        // Create HDF5 Dataset
        hsize_t buffersize = outbuffer.size();
        try {
            clockstart = clock();
            H5LTmake_dataset(hdf5_out, padTo(frame_number, (size_t) 6).c_str(), 1, &buffersize, H5T_NATIVE_UCHAR, &outbuffer[0]);
            duration = 100 * ( clock() - clockstart ) / (double) CLOCKS_PER_SEC;
            LOG(INFO) << "Write to HDF5: " << duration << "ms";
            frame_number++;
        } catch (...) {
            LOG(INFO) << "Frame dropped from HDF5 creation";
        }
    }


    // Conversion of status from nlohmann to bsoncxx
    // nlohmann::json status = GetStatus();
    // string camerainfo = status.dump();

    // Final update
    /*
    bsoncxx::stdx::optional<bsoncxx::document::value> maybe_result = task.update_one(bsoncxx::builder::stream::document{}
        << "_id" << bsoncxx::oid(taskid.c_str())
        << bsoncxx::builder::stream::finalize
        << bsoncxx::builder::stream::document{}
            << "$set" << bsoncxx::builder::stream::open_document 
                << "image" << outname
        << bsoncxx::builder::stream::close_document
        << bsoncxx::builder::stream::finalize);
    */

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
 * Create a task entry in the database for an HDF5 file
 */
void AgriDataCamera::AddTask(string hdf5file) {
    // New Mongo Connection
    mongocxx::client _conn{mongocxx::uri{"mongodb://localhost:27017"}};
    mongocxx::database _db = _conn["plenty"];
    mongocxx::collection _pretask = _db["pretask"];
    mongocxx::collection _box = _db["box"];
    int priority;

    // Create the document (Stream Builder is not appropriate because the construction is broken up)
    bsoncxx::builder::basic::document builder{};

    // Every task gets these fields
    builder.append(bsoncxx::builder::basic::kvp("clientid", clientid));
    builder.append(bsoncxx::builder::basic::kvp("scanid", scanid));
    builder.append(bsoncxx::builder::basic::kvp("hdf5filename", hdf5file));
    builder.append(bsoncxx::builder::basic::kvp("cameraid", serialnumber));
    builder.append(bsoncxx::builder::basic::kvp("session_name", session_name));
    builder.append(bsoncxx::builder::basic::kvp("cluster_detection", 0));

    // If calibration. . .
    if (T_CALIBRATION-- > 0) {
        priority = 0;
    } else {
        // Get highest priority and increment by one
        auto order = bsoncxx::builder::stream::document{} << "priority" << -1 << bsoncxx::builder::stream::finalize;
        auto opts = mongocxx::options::find{};
        opts.sort(order.view());
        bsoncxx::stdx::optional<bsoncxx::document::value> val = _pretask.find_one({}, opts);

        if (val) {
            priority = nlohmann::json::parse(bsoncxx::to_json(*val))["priority"];
            ++priority;
        } else {
            // Special case (first task in the database)
            priority = 1;
        }

        // Non-calibration pretask also receive these fields
        builder.append(bsoncxx::builder::basic::kvp("preprocess", 0));
        builder.append(bsoncxx::builder::basic::kvp("trunk_detection", 0));
        builder.append(bsoncxx::builder::basic::kvp("process", 0));
        builder.append(bsoncxx::builder::basic::kvp("shape_analysis_per_archive", 0));

    }

    // Set the priority
    builder.append(bsoncxx::builder::basic::kvp("priority", priority));

    // Close and insert the document
    bsoncxx::document::value document = builder.extract();
    auto ret = _pretask.insert_one(document.view());
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
 * Snap
 *
 * Snap will take one photo, in isolation, and save it to the standard steaming
 * image location.
 *
 * Consider making this json instead of void to return success
 */
void AgriDataCamera::snapCycle() {
    while (true) {
        try {
            CPylonImage image;
            Mat snap_img = Mat(width, height, CV_8UC3);
            CGrabResultPtr ptrGrabResult;

            RetrieveResult(50000, ptrGrabResult, TimeoutHandling_ThrowException);
            
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

    try {
        LOG(INFO) << "Dumping last " << documents.size() << " documents to Database";
        frames.insert_many(documents);
        documents.clear();
    } catch(...) {
        LOG(WARNING) << "Dumping failed";
    }

    // Close AVI
    try {
        LOG(INFO) << "Closing active AVI file";
        oVideoWriter.release();
    } catch(const GenericException &e) {
        LOG(WARNING) << "Closing file failed";
    }

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