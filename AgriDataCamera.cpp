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
using json = nlohmann::json;


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
    db = conn["agdb"];
    frames = db["frame"];

    // Open camera object ahead of time
    // When stopping and restarting the camera, either one must Close() or otherwise
    // check that the camera is not already open
    if (!IsOpen()) {
        Open();
    }

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

    // Color Format
    COLOR_FMT = "RGB";
    
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


    // Box Information
    LOG(INFO) << "Obtaining Box Information";
    mongocxx::collection box = db["box"];
    bsoncxx::stdx::optional<bsoncxx::document::value> maybe_result = box.find_one(bsoncxx::builder::stream::document{}<< bsoncxx::builder::stream::finalize);
    string resultstring = bsoncxx::to_json(*maybe_result);
    auto thisbox = json::parse(resultstring);
    clientid = thisbox["clientid"];
    try {
        if (thisbox["cameras"][serialnumber].get<string>().compare("Left")) {
            rotation = "ROTATE_90_COUNTERCLOCKWISE";
        } else if (thisbox["cameras"][serialnumber].get<string>().compare("Right")) {
            rotation = "ROTATE_90_CLOCKWISE";
        } else {
            rotation = "";
        }
    } catch (...) {
        LOG(WARNING) << "Unable to determine camera orientation";
        rotation = "";
    }

    // Frame Rate
    // HIGH_FPS = (float) CFloatPtr(nodeMap.GetNode("ResultingFrameRateAbs"))->GetValue();
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
    LOG(INFO) << "Rotation required : " << rotation << " degrees";
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
    bsoncxx::stdx::optional<bsoncxx::document::value> val = frames.find_one(bsoncxx::builder::stream::document{} << "serialnumber" << serialnumber << "scanid" << scanid << bsoncxx::builder::stream::finalize, opts);

    if (val) {
        LOG(DEBUG) << "Obtaining Frame Number";
        LOG(DEBUG) << bsoncxx::to_json(*val);
        // Increment
        frame_number = json::parse(bsoncxx::to_json(*val))["frame_number"];
        LOG(DEBUG) << bsoncxx::to_json(*val);
        LOG(DEBUG) << "Last number found: " << frame_number;;
        return frame_number++;
    } else {
        // Or start
        return 1;
    }
}


/**
 * Run
 *
 * Main loop
 */
void AgriDataCamera::Run() {
    // Output parameters
    save_prefix = "/data/output/" + clientid + "/" + scanid + "/" + serialnumber + "/";
    bool success = AGDUtils::mkdirp(save_prefix.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    // Get current frame number
    frame_number = GetFrameNumber(scanid);

    // Set recording to true and start grabbing
    isRecording = true;
    if (!IsGrabbing()) {
        StartGrabbing();
    }

    // Save configuration
    INodeMap &nodeMap = GetNodeMap();
    string config = save_prefix + "config.txt";
    CFeaturePersistence::Save(config.c_str(), &nodeMap);
    
    // Initiate main loop with algorithm
    while (isRecording) {
        // Wait for an image and then retrieve it. A timeout of 5000 ms is used.
        RetrieveResult(5000, ptrGrabResult, TimeoutHandling_ThrowException);
        try {
            // Image grabbed successfully?
            if (ptrGrabResult->GrabSucceeded()) {
                // Create Frame Packet
                FramePacket fp;

                // Computer time
                fp.time_now = AGDUtils::grabMilliseconds();
                last_timestamp = fp.time_now;

                // Exposure time
                try { // USB
                    fp.exposure_time = (float) CFloatPtr(GetNodeMap().GetNode("ExposureTime"))->GetValue();
                } catch (...) { // GigE
                    fp.exposure_time = (float) CFloatPtr(GetNodeMap().GetNode("ExposureTimeAbs"))->GetValue();
                }

                // Image
                fp.img_ptr = ptrGrabResult;

                // Process the frame
                try {
                    HandleFrame(fp);
                } catch (...) {
                    LOG(WARNING) << "Frame slipped!";
                }

            } else {
                LOG(ERROR) << "Error: " << ptrGrabResult->GetErrorCode() << " " << ptrGrabResult->GetErrorDescription();
                LOG(WARNING) << serialnumber << " is stressed! Slowing down to " << LOW_FPS;
                try {
                    RT_PROBATION = PROBATION;
                    AcquisitionFrameRateEnable.SetValue(true);
                    AcquisitionFrameRateAbs.SetValue(LOW_FPS);
                    LOG(DEBUG) << "Changed Successfully";
                    // fps->SetValue(LOW_FPS); 
                } catch (const GenericException &e) {
                    LOG(DEBUG) << "Passing on exception: " << e.GetDescription();
                }
            }
        } catch (const GenericException &e) {
            LOG(ERROR) << ptrGrabResult->GetErrorCode() + "\n"
                    + ptrGrabResult->GetErrorDescription() + "\n"
                    + e.GetDescription();
            isRecording = false;
        }
    }
}


/**
 * HandleFrame
 *
 * Receive latest frame
 */
void AgriDataCamera::HandleFrame(AgriDataCamera::FramePacket fp) {
    double dif;
    struct timeval tp;
    long int start, end;
    tick++;

    // Need the latest nodemap? Not sure whether or not this can / should be a private member
    INodeMap &nodeMap = GetNodeMap();

    // Docuemnt
    auto doc = bsoncxx::builder::basic::document{};
    doc.append(
            bsoncxx::builder::basic::kvp("serialnumber", serialnumber));
    doc.append(bsoncxx::builder::basic::kvp("scanid", scanid));

    // Basler time and frame
    ostringstream camera_time;
    camera_time << fp.img_ptr->GetTimeStamp();
    doc.append(bsoncxx::builder::basic::kvp("camera_time", (string) camera_time.str()));
    doc.append(bsoncxx::builder::basic::kvp("timestamp", fp.time_now));
    doc.append(bsoncxx::builder::basic::kvp("frame_number", frame_number));

    // Add Camera data
    doc.append(bsoncxx::builder::basic::kvp("exposure_time", fp.exposure_time));

    // Computer time and output directory
    vector<string> hms = AGDUtils::split(AGDUtils::grabTime("%H:%M:%S"), ':');
    string hdf5file = scanid + "_" + serialnumber + "_" + hms[0].c_str() + "_" + hms[1].c_str() + "_" + COLOR_FMT + ".hdf5";
    doc.append(bsoncxx::builder::basic::kvp("filename", hdf5file));

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
        string VERSION = "1.2";
        H5LTset_attribute_string(hdf5_out, "/", "COLOR_FMT", COLOR_FMT.c_str());
        H5LTset_attribute_string(hdf5_out, "/", "VERSION", VERSION.c_str());
        H5LTset_attribute_string(hdf5_out, "/", "ROTATION_NEEDED", rotation.c_str());
    }


    // Convert to BGR8Packed CPylonImage
    fc.Convert(image, fp.img_ptr);

    // To OpenCV Mat
    last_img = Mat(fp.img_ptr->GetHeight(), fp.img_ptr->GetWidth(), CV_8UC3, (uint8_t *) image.GetBuffer());

    // Resize
    resize(last_img, small_last_img, Size(TARGET_HEIGHT, TARGET_WIDTH));

    // Color
    if (COLOR_FMT.compare("RGB")) {
        cvtColor(small_last_img, small_last_img, CV_BGR2RGB);
    }

    // Encode to JPG Buffer
    vector<uint8_t> outbuffer;
    static const vector<int> ENCODE_PARAMS = {};
    imencode(".jpg", small_last_img, outbuffer, ENCODE_PARAMS);

    // Create HDF5 Dataset
    hsize_t buffersize = outbuffer.size();
    try {
        H5LTmake_dataset(hdf5_out, padTo(frame_number, (size_t) 6).c_str(), 1, &buffersize, H5T_NATIVE_UCHAR, &outbuffer[0]);
        frame_number++;
    } catch (...) {
        LOG(INFO) << "Frame dropped (likely end of recording)";
    }

    // Write to streaming image
    if (tick % T_LATEST == 0) {
        thread t(&AgriDataCamera::writeLatestImage, this, last_img,
                ref(compression_params));
        t.detach();
    }

    // Dynamic frame rate adjustment
    RT_PROBATION--;
    if (RT_PROBATION == 0) {       // Transition (special case)
          AcquisitionFrameRateEnable.SetValue(true);
          AcquisitionFrameRateAbs.SetValue(HIGH_FPS);  
          LOG(DEBUG) << "Returning to " << HIGH_FPS << " FPS";
    } else if (RT_PROBATION < -1) {         
          RT_PROBATION=-1;        // Back to normal
    }

    // Check Luminance (and add to documents)
    if (tick % T_LUMINANCE == 0) {
        // We send to database first, then we can edit it later
        auto ret = frames.insert_one(doc.view());
        bsoncxx::oid oid = ret->inserted_id().get_oid().value;
        thread t(&AgriDataCamera::Luminance, this, oid, small_last_img);
        t.detach();
    } else {
        // Add to documents
        documents.push_back(doc.extract());
    }

    // Send documents to database
    if ((tick % T_MONGODB == 0) && (documents.size() > 0)) {
        LOG(DEBUG) << "Sending " << documents.size() << " documents to Database";
        frames.insert_many(documents);
        documents.clear();
    }
}


/**
 * AddTask
 *
 * Create a task entry in the database for an HDF5 file
 */
void AgriDataCamera::AddTask(string hdf5file) {
    // New Mongo Connection
    mongocxx::client _conn{mongocxx::uri{"mongodb://localhost:27017"}};
    mongocxx::database _db = _conn["agdb"];
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
            priority = json::parse(bsoncxx::to_json(*val))["priority"];
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
    mongocxx::database _db = _conn["agdb"];
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
        snap_img = Mat(ptrGrabResult->GetHeight(), ptrGrabResult->GetWidth(),
                CV_8UC3, (uint8_t *) image.GetBuffer());

        snap_img.copyTo(last_img);
        thread t(&AgriDataCamera::writeLatestImage, this, last_img,
                ref(compression_params));
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
    Mat thumb;
    resize(img, thumb, Size(), 0.3, 0.3);

    // Thumbnail
    imwrite(
            "/data/EmbeddedServer/images/" + serialnumber + '_'
            + "streaming_t.jpg", thumb, compression_params);
    // Full
    /*
    imwrite(
            "/data/EmbeddedServer/images/" + serialnumber + '_'
            + "streaming.jpg", img, compression_params);
    */

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
json AgriDataCamera::GetStatus() {
    json status;
    INodeMap &nodeMap = GetNodeMap();

    status["Serial Number"] = serialnumber;
    status["Model Name"] = modelname;
    status["Recording"] = isRecording;

    // Something funny here, occasionally the ptrGrabResult is not available
    // even though the camera is grabbing?
    if (isRecording) {
        status["Timestamp"] = last_timestamp;
        status["scanid"] = scanid;
    } else {
        status["Timestamp"] = 0;
        status["scanid"] = "Not Recording";
    }

    // Here is the main divergence between GigE and USB Cameras; the nodemap is not standard
    try { // USB
        status["Current Gain"] = (float) CFloatPtr(nodeMap.GetNode("Gain"))->GetValue();
        status["Exposure Time"] = (float) CFloatPtr(nodeMap.GetNode("ExposureTime"))->GetValue();
        status["Resulting Frame Rate"] = (float) CFloatPtr(nodeMap.GetNode("ResultingFrameRate"))->GetValue();
        status["Temperature"] = (float) CFloatPtr(nodeMap.GetNode("DeviceTemperature"))->GetValue();
        status["Target Brightness"] = (int) CFloatPtr(nodeMap.GetNode("AutoTargetBrightness"))->GetValue();
    } catch (...) { // GigE
        status["Current Gain"] = (int) CIntegerPtr(nodeMap.GetNode("GainRaw"))->GetValue(); // Gotcha!
        status["Exposure Time"] = (float) CFloatPtr(nodeMap.GetNode("ExposureTimeAbs"))->GetValue();
        status["Red Balance"] = (float) CFloatPtr(nodeMap.GetNode("BalanceRatioSelector_Red"))->GetValue();
        status["Green Balance"] = (float) CFloatPtr(nodeMap.GetNode("BalanceRatioSelector_Blue"))->GetValue();
        status["Blue Balance"] = (float) CFloatPtr(nodeMap.GetNode("BalanceRatioSelector_Green"))->GetValue();
        status["Resulting Frame Rate"] = (float) CFloatPtr(nodeMap.GetNode("ResultingFrameRateAbs"))->GetValue();
        status["Temperature"] = (float) CFloatPtr(nodeMap.GetNode("TemperatureAbs"))->GetValue();
        status["Target Brightness"] = (int) CIntegerPtr(nodeMap.GetNode("AutoTargetValue"))->GetValue();
        status["Target Frame Rate"] = AcquisitionFrameRateAbs.GetValue();
        status["Probation"] = RT_PROBATION;
    }

    bsoncxx::document::value document = bsoncxx::builder::stream::document{}  
            << "Serial Number" << (string) status["Serial Number"].get<string>()
            << "Model Name" << (string) status["Model Name"].get<string>()
            << "Recording" << (bool) status["Recording"].get<bool>()
            << "Timestamp" << (int64_t) status["Timestamp"].get<int64_t>()
            << "scanid" << (string) status["scanid"].get<string>()
            << "Exposure Time" << (int) status["Exposure Time"].get<int>()
            << "Resulting Frame Rate" << (int) status["Resulting Frame Rate"].get<int>()
            << "Target Frame Rate" << status["Target Frame Rate"].get<float>()
            << "Current Gain" << (int) status["Current Gain"].get<int>()
            << "Temperature" << (int) status["Temperature"].get<int>()
            << "Target Brightness" << (int) status["Target Brightness"].get<int>()
            << "Probation" << (int) status["Probation"].get<int>()
            << bsoncxx::builder::stream::finalize;

    // Insert into the DB
    auto ret = frames.insert_one(document.view());

    // Lazily add luminance (bit of delay here, but checking luminance is done via DB by clients that want it)
    if (!isRecording) {
        // Grab an image for luminance calculation (this will set last_image
        // . . . so the call to Luminance doesn't need it as an argument
        AgriDataCamera::Snap();

        // Add the luminance value
        bsoncxx::oid oid = ret->inserted_id().get_oid().value;
        thread t(&AgriDataCamera::Luminance, this, oid, last_img);
        t.detach();
    }

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
