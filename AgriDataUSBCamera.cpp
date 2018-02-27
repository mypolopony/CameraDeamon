/*
 * File:   AgriDatacpp
 * Author: agridata
 *
 * Created on March 13, 2017, 1:33 PM
 */

#include "AgriDataUSBCamera.h"
#include "AGDUtils.h"

// Utilities
#include "zmq.hpp"
#include "zhelpers.hpp"
#include "json.hpp"

// MongoDB & BSON
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>

// Include files to use the PYLON API.
#include <pylon/PylonIncludes.h>
#include <pylon/usb/BaslerUsbInstantCamera.h>
#include <pylon/usb/BaslerUsbInstantCameraArray.h>
#include <pylon/usb/_BaslerUsbCameraParams.h>

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
#include <condition_variable>

// System
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

// Include files to use openCV
#include "opencv2/core.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"

// Redis
#include <redox.hpp>

// Namespaces
using namespace Basler_UsbCameraParams;
using namespace Pylon;
using namespace std;
using namespace cv;
using namespace GenApi;
using json = nlohmann::json;

/**
 * Constructor
 */
AgriDataUSBCamera::AgriDataUSBCamera() :
    ctx_(1), imu_(ctx_, ZMQ_REQ), conn { mongocxx::uri {
        MONGODB_HOST
    }
} {
}

/**
 * Destructor
 */
AgriDataUSBCamera::~AgriDataUSBCamera()
{
}

/**
 * Initialize
 *
 * Opens the camera and initializes it with some settings
 */
void AgriDataUSBCamera::Initialize()
{
    PylonAutoInitTerm autoInitTerm;

    // Open camera object ahead of time
    // When stopping and restarting the camera, either one must Close() or otherwise
    // check that the camera is not already open
    if (!IsOpen()) {
        Open();
    }

    if (!IsGrabbing()) {
        StartGrabbing();
    }

    INodeMap& nodeMap = GetNodeMap();

    // Print the model name of the
    cout << "Initializing device " << GetDeviceInfo().GetModelName() << endl;

    try {
        string config = "/home/nvidia/CameraDeamon/config/"
                        + string(GetDeviceInfo().GetModelName()) + ".pfs";
        cout << "Reading from configuration file: " + config;
        cout << endl;
        CFeaturePersistence::Load(config.c_str(), &nodeMap, true);

    } catch (const GenericException &e) {
        cerr << "An exception occurred." << endl << e.GetDescription() << endl;
    }

    /*
     int frames_per_second = 20;
     int exposure_lower_limit = 52;
     int exposure_upper_limit = 1200;

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
     // GainAutoEnable.SetValue(true);
     GainAuto.SetValue(GainAuto_Continuous);
     AutoGainUpperLimit.SetValue(6);
     AutoGainLowerLimit.SetValue(0);
     */

    // Number of buffers does not seem to be specified in .pfs file
    // I'm pretty sure the max is 10, so I don't think any other values are valid.
    // Also, this seems to raise trouble when the cameras has already been initialized,
    // i.e. on stop / reinitialization
    // try {
    //    GetStreamGrabberParams().MaxNumBuffer.SetValue(256);
    // } catch (...) {
    //     cerr << "MaxNumBuffer already set" << endl;
    // }
    // Get Dimensions
    width = Width.GetValue();
    height = Height.GetValue();

    // Print camera device information.
    cout << "Camera Device Information" << endl << "========================="
         << endl;
    cout << "Vendor : "
         << CStringPtr(GetNodeMap().GetNode("DeviceVendorName"))->GetValue()
         << endl;
    cout << "Model : "
         << CStringPtr(GetNodeMap().GetNode("DeviceModelName"))->GetValue()
         << endl;
    cout << "Firmware version : "
         << CStringPtr(GetNodeMap().GetNode("DeviceFirmwareVersion"))->GetValue()
         << endl;
    cout << "Serial Number : "
         << CStringPtr(GetNodeMap().GetNode("DeviceID"))->GetValue() << endl;
    cout << "Frame Size  : " << width << 'x' << height << endl << endl;

    // Create Mat image templates
    cv_img = Mat(width, height, CV_8UC3);
    last_img = Mat(width, height, CV_8UC3);
    resize(last_img, small_last_img, Size(), 0.5, 0.5);

    // Define pixel output format (to match algorithm optimalization)
    fc.OutputPixelFormat = PixelType_BGR8packed;

    // ZMQ and DB Connection
    imu_.connect("tcp://127.0.0.1:4997");

    // Wait for sockets
    zmq_sleep(1.5);

    // Initialize MongoDB connection
    // The use of auto here is unfortunate, but it is apparently recommended
    // The type is actually N8mongocxx7v_noabi10collectionE or something crazy
    db = conn["agdb"];
    frames = db["frame"];

    // Initial status
    isRecording = false;
    isPaused = false;

    // Timer
    tick = 0;

    // Streaming image compression
    compression_params.push_back(CV_IMWRITE_PNG_COMPRESSION);
    compression_params.push_back(3);
    compression_params.push_back(3);
    compression_params.push_back(3);

    // Identifier
    serialnumber = (string) DeviceID();

    // Obtain box info to determine camera rotation
    mongocxx::collection box = db["box"];
    bsoncxx::stdx::optional<bsoncxx::document::value> maybe_result = box.find_one(bsoncxx::builder::stream::document {} << bsoncxx::builder::stream::finalize);
    try {
        string resultstring = bsoncxx::to_json(*maybe_result);
        auto thisbox = json::parse(resultstring);
        if (thisbox["cameras"][serialnumber].get<string>().compare("Left")) {
            rotation = -90;
        } else if (thisbox["cameras"][serialnumber].get<string>().compare("Right")) {
            rotation = 90;
        } else {
            rotation = 0;
        }
    } catch (...) {
        cerr << "Unable to determine camera orientation. . ." << endl;
        cerr << "Setting rotation to 0" << endl;
        rotation = 0;
    }

    // HDF5
    current_hdf5_file = "";
}

/**
 * imu_wrapper
 *
 * Allows a timeout to be attached to the IMU call
 */
string AgriDataUSBCamera::imu_wrapper(AgriDataUSBCamera::FramePacket fp)
{
    mutex m;
    condition_variable cv;
    string data;

    thread t([this, &m, &cv, &data]() {
        s_send(imu_, " ");
        data = s_recv(imu_);
        cv.notify_one();
    });

    t.detach();

    {
        unique_lock < mutex > lock(m);
        if (cv.wait_for(lock, chrono::milliseconds(20)) == cv_status::timeout)
            throw runtime_error("IMU Timeout");
    }

    return data;
}

/**
 * Run
 *
 * Main loop
 */
void AgriDataUSBCamera::Run()
{
    // Strings and streams
    string videofile;
    string framefile;
    string logmessage;
    string config;

    // Output parameters
    save_prefix = "/data/output/" + scanid + "/"
                  + GetDeviceInfo().GetMacAddress() + "/";
    bool success = AGDUtils::mkdirp(save_prefix.c_str(),
                                    S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    /*
     // Filestatus for periodically checking filesize
     struct stat filestatus;

     // Open the video file
     string timenow = AGDUtils::grabTime("%H_%M_%S");
     videofile = save_prefix + timenow + ".avi";
     videowriter = VideoWriter(videofile.c_str(), CV_FOURCC('M', 'J', 'P', 'G'), AcquisitionFrameRate.GetValue(),
     Size(width, height), true);

     // Make sure videowriter was opened successfully
     if (videowriter.isOpened()) {
     logmessage = "Opened video file: " + videofile;
     syslog(LOG_INFO, logmessage.c_str());
     } else {
     logmessage = "Failed to write the video file: " + videofile;
     syslog(LOG_ERR, logmessage.c_str());
     }

     framefile =  save_prefix + ".txt";
     frameout.open(framefile);
     if (frameout.is_open()) {
     logmessage = "Opened log file: " + framefile;
     syslog(LOG_INFO, logmessage.c_str());
     writeHeaders();
     } else {
     logmessage = "Failed to open log file: " + framefile;
     syslog(LOG_ERR, logmessage.c_str());
     }
     */

    // Set recording to true and start grabbing
    isRecording = true;
    if (!IsGrabbing()) {
        StartGrabbing();
    }

    // Save configuration
    INodeMap& nodeMap = GetNodeMap();
    config = save_prefix + "config.txt";
    CFeaturePersistence::Save(config.c_str(), &nodeMap);

    // Timing
    struct timeval tim;

    // initiate main loop with algorithm
    while (isRecording) {
        if (!isPaused) {
            // Wait for an image and then retrieve it. A timeout of 5000 ms is used.
            RetrieveResult(5000, ptrGrabResult, TimeoutHandling_ThrowException);
            try {
                // Image grabbed successfully?
                if (ptrGrabResult->GrabSucceeded()) {
                    // Create Frame Packet
                    FramePacket fp;

                    // Computer time
                    fp.time_now = AGDUtils::grabTime("%H:%M:%S");
                    last_timestamp = fp.time_now;

                    // IMU data
                    /*
                    try {
                        fp.imu_data = imu_wrapper(fp);
                    } catch(runtime_error& e) {
                        fp.imu_data = "{}";
                        cout << "IMU Error: " << e.what() << endl;
                    }
                     * */
                    fp.imu_data = "{}";

                    // Exposure time
                    fp.exposure_time = ExposureTimeRaw();

                    // Image
                    fp.img_ptr = ptrGrabResult;

                    HandleFrame(fp);
                } else {
                    cout << "Error: " << ptrGrabResult->GetErrorCode() << " "
                         << ptrGrabResult->GetErrorDescription() << endl;
                }
            } catch (const GenericException &e) {
                logmessage = ptrGrabResult->GetErrorCode() + "\n"
                             + ptrGrabResult->GetErrorDescription() + "\n"
                             + e.GetDescription();
                syslog(LOG_ERR, logmessage.c_str());
                isRecording = false;
            }
        }
    }
}

/**
 * HandleFrame
 *
 * Receive latest frame
 */
void AgriDataUSBCamera::HandleFrame(AgriDataUSBCamera::FramePacket fp)
{
    double dif;
    struct timeval tp;
    long int start, end;
    tick++;

    // Docuemnt
    auto doc = bsoncxx::builder::basic::document { };
    doc.append(
        bsoncxx::builder::basic::kvp("serialnumber", serialnumber));
    doc.append(bsoncxx::builder::basic::kvp("scanid", scanid));

    // Basler time and frame
    ostringstream camera_time;
    camera_time << fp.img_ptr->GetTimeStamp();
    doc.append(bsoncxx::builder::basic::kvp("camera_time", (string) camera_time.str() ));
    doc.append(
        bsoncxx::builder::basic::kvp("frame_number",
                                     fp.img_ptr->GetImageNumber()));

    // Parse IMU data
    try {
        json frame_obj = json::parse(fp.imu_data);
        for (json::iterator it = frame_obj.begin(); it != frame_obj.end();
             ++it) {
            try {
                doc.append(
                    bsoncxx::builder::basic::kvp((string) it.key(),
                                                 (double) it.value()));
            } catch (...) {
                doc.append(
                    bsoncxx::builder::basic::kvp((string) it.key(),
                                                 (bool) it.value()));
            }
        }
    } catch (...) {
        cerr << "Sorry, no IMU information is available!\n";
    }

    // Add Camera data
    doc.append(bsoncxx::builder::basic::kvp("balance_red", fp.balance_red));
    doc.append(bsoncxx::builder::basic::kvp("balance_green", fp.balance_green));
    doc.append(bsoncxx::builder::basic::kvp("balance_blue", fp.balance_blue));
    doc.append(bsoncxx::builder::basic::kvp("exposure_time", fp.exposure_time));

    // Computer time and output directory
    vector<string> hms = AGDUtils::split(fp.time_now, ':');
    string hdf5file = save_prefix + hms[0].c_str() + "_" + hms[1].c_str() + ".hdf5";

    // Should we open a new file?
    if (hdf5file.compare(current_hdf5_file) != 0) {
        // Close the previous file (if it is a thing)
        if (current_hdf5_file.compare("") != 0) {
            H5Fclose( hdf5_output );

            // Add to queue
            redox::Redox rdx;
            if (rdx.connect() == 1) {
                rdx.command<string>( {"LPUSH", "detection", current_hdf5_file});
            }
        }
        hdf5_output = H5Fcreate( hdf5file.c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT );
        current_hdf5_file = hdf5file;
    }

    doc.append(bsoncxx::builder::basic::kvp("filename", current_hdf5_file));

    // Convert
    fc.Convert(image, fp.img_ptr);

    // To OpenCV
    last_img = Mat(fp.img_ptr->GetHeight(), fp.img_ptr->GetWidth(), CV_8UC3, (uint8_t *) image.GetBuffer());

    // Resize
    resize(last_img, small_last_img, Size(), 0.5, 0.5);

    // Color Conversion
    cvtColor(small_last_img, small_last_img, CV_BGR2RGB);

    // Rotate
    small_last_img = AgriDataUSBCamera::Rotate( small_last_img );

    // Write
    H5IMmake_image_24bit( hdf5_output, to_string(fp.img_ptr->GetImageNumber()).c_str(), small_last_img.cols, small_last_img.rows, "INTERLACE_PIXEL", small_last_img.data);
    //string stick = to_string(tick);
    //imwrite(save_prefix + serialnumber + stick + ".jpg", small_last_img);

    // Write to streaming image
    if ( tick % T_LATEST == 0) {
        last_img = Mat(fp.img_ptr->GetHeight(), fp.img_ptr->GetWidth(), CV_8UC3,
                       (uint8_t *) image.GetBuffer());

        thread t(&AgriDataUSBCamera::writeLatestImage, this, last_img,
                 ref(compression_params));
        t.detach();
    }

    // Check Luminance (and add to documents)
    if ( tick % T_LUMINANCE == 0) {

        // We send to database first, then we can edit it later
        auto ret = frames.insert_one(doc.view());
        bsoncxx::oid oid = ret->inserted_id().get_oid().value;
        thread t(&AgriDataUSBCamera::Luminance, this, oid, small_last_img);
        t.detach();
    } else {
        // Add to documents
        documents.push_back(doc.extract());
    }

    // Send documents to database
    if (( tick % T_MONGODB  == 0) && (documents.size() > 0)) {
        frames.insert_many(documents);
        documents.clear();
    }
}

/**
 * Rotation
 *
 * Rotate the input image by an angle (the proper way!)
 */
Mat AgriDataUSBCamera::Rotate(Mat input)
{
    // Get rotation matrix for rotating the image around its center
    cv::Point2f center(input.cols/2.0, input.rows/2.0);
    cv::Mat rot = cv::getRotationMatrix2D(center, rotation, 1.0);

    // Determine bounding rectangle
    cv::Rect bbox = cv::RotatedRect(center, input.size(), rotation).boundingRect();

    // Adjust transformation matrix
    rot.at<double>(0,2) += bbox.width/2.0 - center.x;
    rot.at<double>(1,2) += bbox.height/2.0 - center.y;

    // Resultant
    cv::Mat dst;
    cv::warpAffine(input, dst, rot, bbox.size());

    return dst;
}

/**
 * Luminance
 *
 * Return luminance for an OpenCV Mat (frame)
 *
 */
void AgriDataUSBCamera::Luminance(bsoncxx::oid id, cv::Mat input)
{
    // As per http://mongodb.github.io/mongo-cxx-driver/mongocxx-v3/thread-safety/
    // "don't even bother sharing clients. Just give each thread its own"
    mongocxx::client _conn { mongocxx::uri { MONGODB_HOST } };
    mongocxx::database _db = _conn["agdb"];
    mongocxx::collection _frames = _db["frame"];

    cv::Mat grayMat;
    cv::cvtColor(input, grayMat, CV_BGR2GRAY);

    // Summation of intensity
    int Totalintensity = 0;
    for (int i=0; i < grayMat.rows; ++i) {
        for (int j=0; j < grayMat.cols; ++j) {
            Totalintensity += (int)grayMat.at<uchar>(i, j);
        }
    }

    // Find avg lum of frame
    float avgLum = Totalintensity/(grayMat.rows * grayMat.cols);

    // Update database entry
    _frames.update_one(bsoncxx::builder::stream::document {} << "_id" << id << bsoncxx::builder::stream::finalize,
                       bsoncxx::builder::stream::document {} << "$set" << bsoncxx::builder::stream::open_document <<
                       "luminance" << avgLum << bsoncxx::builder::stream::close_document << bsoncxx::builder::stream::finalize);
}


/**
 * Snap
 *
 * Snap will take one photo, in isolation, and save it to the standard steaming
 * image location.
 *
 * Consider making this json instead of void to return success
 *
 */

void AgriDataUSBCamera::Snap()
{
    // this !isRecording criterion is enforced because I don't know what the camera's
    // behavior is to ask for one frame while another (continuous) grabbing process is
    // ongoing, and really I don't think there should be a need for such feature.
    if (!isRecording) {
        CPylonImage image;
        Mat snap_img = Mat(width, height, CV_8UC3);
        CGrabResultPtr ptrGrabResult;

        // There might be a reason to allow the camera to take a few shots first to
        // allow any auto adjustments to take place.
        uint32_t c_countOfImagesToGrab = 20;

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
        thread t(&AgriDataUSBCamera::writeLatestImage, this, last_img,
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
void AgriDataUSBCamera::writeLatestImage(Mat img, vector<int> compression_params)
{
    string snumber;
    snumber = (string) DeviceID();

    Mat thumb;
    resize(img, thumb, Size(), 0.2, 0.2);

    // Thumbnail
    imwrite(
        "/home/nvidia/EmbeddedServer/images/" + snumber + '_'
        + "streaming_t.png", thumb, compression_params);
    // Full
    //imwrite(
    //		"/home/nvidia/EmbeddedServer/images/" + snumber + '_'
    //				+ "streaming.png", img, compression_params);

}

/**
 * Stop
 *
 * Upon receiving a stop message, set the isRecording flag
 */
int AgriDataUSBCamera::Stop()
{

    syslog(LOG_INFO, "Recording Stopped");
    isRecording = false;

    cout << "Closing active HDF5 file" << endl;
    H5Fclose( hdf5_output );

    cout << "Dumping documents" << endl;
    frames.insert_many(documents);
    documents.clear();

    syslog(LOG_INFO, "*** Done ***");
    frameout.close();
    return 0;
}

/**
 * GetStatus
 *
 * Respond to the heartbeat the data about the camera
 */
json AgriDataUSBCamera::GetStatus()
{
    json status, imu_status;
    string docstring;
    imu_status["IMU_VELOCITY_NORTH"] = -99.9;
    imu_status["IMU_VELOCITY_EAST"] = -99.9;

    // If we are not recording, make a new IMU request
    // If we are recording, this can get in the way of the frame grabber's communication with the imu
    /*
    if (!isRecording) {
        s_send(imu_, " ");
        try {
            imu_status = json::parse(s_recv(imu_));
        } catch (...) {
            cerr << "Bad IMU\n";
        }
    } else {
        if (!imu_status.is_null()) {
            try {
                imu_status = json::parse(last_imu_data);
            } catch (...) {
                cerr << "IMU Fail\n";
            }
        }
    }

    try {
        status["Velocity_North"] =
            imu_status["IMU_VELOCITY_NORTH"].get<double>();
        status["Velocity_East"] = imu_status["IMU_VELOCITY_EAST"].get<double>();
    } catch (...) {
        cout << "Second IMU Fail";
    }
    */

    status["Serial Number"] = (string) DeviceID();
    status["Model Name"] = (string) GetDeviceInfo().GetModelName();
    status["Recording"] = isRecording;

    // Something funny here, occasionally the ptrGrabResult is not available
    // even though the camera is grabbing?
    if (isRecording) {
        status["Timestamp"] = last_timestamp;
        status["scanid"] = scanid;
    } else {
        status["Timestamp"] = "Not Recording";
        status["scanid"] = "Not Recording";
    }

    status["Exposure Time"] = ExposureTimeAbs();
    status["Resulting Frame Rate"] = ResultingFrameRateAbs();
    status["Current Gain"] = GainRaw();
    status["Temperature"] = TemperatureAbs();

    // Create BSON from JSON and send to database (if not recording)
    // The stream builder is preferred so we use that (as opposed to the "basic"
    // builder used in the main thread)
    if (!isRecording) {
        // Grab an image for luminance calculation
        // This will set last_image
        AgriDataUSBCamera::Snap();

        // It would be nice to iterate automatically
        // but how to cast json &val?
        /*
        for (auto it = status.begin(); it != status.end(); ++it) {
            const string &key = it.key();
            json &val = it.value();
            doc << key << val;
        }
        */

        bsoncxx::document::value doc = bsoncxx::builder::stream::document {} << "Serial Number" << (string) status["Serial Number"].get<string>()
                                       << "Model Name" << (string) status["Model Name"].get<string>()
                                       << "Recording" << (bool) status["Recording"].get<bool>()
                                       << "Timestamp" << (string) status["Timestamp"].get<string>()
                                       << "scanid" << (string) status["scanid"].get<string>()
                                       << "Exposure Time" << (int) status["Exposure Time"].get<int>()
                                       << "Resulting Frame Rate" << (int) status["Resulting Frame Rate"].get<int>()
                                       << "Current Gain" << (int) status["Current Gain"].get<int>()
                                       << "Temperature" << (int) status["Temperature"].get<int>()
                                       << bsoncxx::builder::stream::finalize;

        auto ret = frames.insert_one(doc.view());
        bsoncxx::oid oid = ret->inserted_id().get_oid().value;
        thread t(&AgriDataUSBCamera::Luminance, this, oid, last_img);
        t.detach();
    }

    return status;
}
