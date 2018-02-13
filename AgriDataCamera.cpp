/*
 * File:   AgriDatacpp
 * Author: agridata
 *
 * Created on March 13, 2017, 1:33 PM
 */

#include "AgriDataCamera.h"
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
#include <condition_variable>

// Other
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

// Include files to use openCV
#include "opencv2/core.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"

// Namespaces
using namespace Basler_GigECameraParams;
using namespace Pylon;
using namespace std;
using namespace cv;
using namespace GenApi;
using json = nlohmann::json;

/**
 * Constructor
 */
AgriDataCamera::AgriDataCamera() :
    ctx_(1), imu_(ctx_, ZMQ_REQ), conn { mongocxx::uri {
        MONGODB_HOST
    }
} {
}

/**
 * Destructor
 */
AgriDataCamera::~AgriDataCamera()
{
}

/**
 * Initialize
 *
 * Opens the camera and initializes it with some settings
 */
void AgriDataCamera::Initialize()
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

    // create Mat image template
    cv_img = Mat(width, height, CV_8UC3);
    last_img = Mat(width, height, CV_8UC3);

    // Define pixel output format (to match algorithm optimalization)
    fc.OutputPixelFormat = PixelType_BGR8packed;
    //persistenceOptions.SetQuality(70);

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

    /*
    // HDF5 output
    h5io = cv::hdf::open( "mytest.h5" );
    // optimise dataset by chunks
    int chunks[1200] = { 960, 600 };
    // create Unlimited x 100 CV_64FC2 space
    h5io->dscreate( 1200, 100, CV_8UC3, "hilbert", cv::hdf::HDF5::H5_NONE, chunks );
    framecounter = 0;
    */
}

/**
 * imu_wrapper
 *
 * Allows a timeout to be attached to the IMU call
 */
string AgriDataCamera::imu_wrapper(AgriDataCamera::FramePacket fp)
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
void AgriDataCamera::Run()
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

                    // Basler timestamp
                    // TODO: This would be attached to fp.img_ptr, by the way. . .
                    ostringstream camera_time;
                    camera_time << ptrGrabResult->GetTimeStamp();
                    fp.camera_time = camera_time.str();

                    // Computer time
                    fp.time_now = AGDUtils::grabTime("%H:%M:%S");
                    last_timestamp = fp.time_now;

                    // IMU data
                    try {
                        fp.imu_data = imu_wrapper(fp);
                    } catch(runtime_error& e) {
                        fp.imu_data = "{}";
                        cout << "IMU Error\n";
                    }

                    /*
                    s_send(imu_, " ");
                    last_imu_data = s_recv(imu_);
                    fp.imu_data = last_imu_data;
                    */

                    // Camera Status
                    //BalanceRatioSelector.SetValue(BalanceRatioSelector_Red);
                    //fp.balance_red = BalanceRatio.GetValue();
                    //BalanceRatioSelector.SetValue(BalanceRatioSelector_Green);
                    //fp.balance_green = BalanceRatio.GetValue();
                    //BalanceRatioSelector.SetValue(BalanceRatioSelector_Blue);
                    //fp.balance_blue = BalanceRatio.GetValue();
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
 * writeHeaders (DEPRECATED)
 *
 * All good logfiles have headers. These are them
 */
void AgriDataCamera::writeHeaders()
{
    ostringstream oss;
    oss << "Timestamp," << "Exposure Time," << "Resulting Frame Rate,"
        << "Current Gain," << "Device Temperature" << endl;

    frameout << oss.str();
}

/**
 * HandleFrame
 *
 * Receive latest frame
 */
void AgriDataCamera::HandleFrame(AgriDataCamera::FramePacket fp)
{
    double dif;
    struct timeval tp;
    long int start, end;
    tick++;

    // fc.Convert(image, fp.img_ptr);
    // cv_img = Mat(fp.img_ptr->GetHeight(), fp.img_ptr->GetWidth(), CV_8UC3,(uint8_t *) image.GetBuffer());

    // Write the original stream into file
    //videowriter << cv_img;

    // Docuemnt
    gettimeofday(&tp, NULL);
    start = tp.tv_usec;
    auto doc = bsoncxx::builder::basic::document { };
    doc.append(
        bsoncxx::builder::basic::kvp("serialnumber", serialnumber));
    doc.append(bsoncxx::builder::basic::kvp("scanid", scanid));

    // Basler time and frame
    doc.append(bsoncxx::builder::basic::kvp("camera_time", fp.camera_time));
    doc.append(
        bsoncxx::builder::basic::kvp("frame_number",
                                     fp.img_ptr->GetImageNumber()));
    gettimeofday(&tp, NULL);
    end = tp.tv_usec;
    cout << "Document generation: " << end-start << endl;

    // Parse IMU data
    gettimeofday(&tp, NULL);
    start = tp.tv_usec;
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
    gettimeofday(&tp, NULL);
    end = tp.tv_usec;
    cout << "Obtain & Parse IMU: " << end-start << endl;

    // Add Camera data
    doc.append(bsoncxx::builder::basic::kvp("balance_red", fp.balance_red));
    doc.append(bsoncxx::builder::basic::kvp("balance_green", fp.balance_green));
    doc.append(bsoncxx::builder::basic::kvp("balance_blue", fp.balance_blue));
    doc.append(bsoncxx::builder::basic::kvp("exposure_time", fp.exposure_time));

    // Computer time and output directory
    vector<string> hms = AGDUtils::split(fp.time_now, ':');
    output_dir = save_prefix + hms[0].c_str() + "/" + hms[1].c_str() + "/";
    bool success = AGDUtils::mkdirp(output_dir.c_str(),S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    //stringstream tarfile;
    //tarfile
    //		<< DeviceID() + "_" + hms[0].c_str() + "_" + hms[1].c_str()
    //				+ ".tar.gz";
    // doc.append(bsoncxx::builder::basic::kvp("filename", tarfile.str()));

    // Save image
    stringstream filename;
    filename << output_dir << fp.img_ptr->GetImageNumber() << ".jpg";

    //CImagePersistence::Save(ImageFileFormat_Tiff, filename.str().c_str(), fp.img_ptr);

    gettimeofday(&tp, NULL);
    start = tp.tv_usec;
    fc.Convert(image, fp.img_ptr);
    gettimeofday(&tp, NULL);
    end = tp.tv_usec;
    cout << "Convert: " << end-start << endl;

    gettimeofday(&tp, NULL);
    start= tp.tv_usec;
    last_img = Mat(fp.img_ptr->GetHeight(), fp.img_ptr->GetWidth(), CV_8UC3,
                   (uint8_t *) image.GetBuffer());
    gettimeofday(&tp, NULL);
    end = tp.tv_usec;
    cout << "ToOpenCVMatrix: " << end-start << endl;

    gettimeofday(&tp, NULL);
    start= tp.tv_usec;
    Mat small_last_img;
    resize(last_img, small_last_img, Size(), 0.5, 0.5);
    gettimeofday(&tp, NULL);
    end = tp.tv_usec;
    cout << "Resize: " << end-start << endl;

    gettimeofday(&tp, NULL);
    start= tp.tv_usec;
    imwrite(filename.str(), small_last_img);
    gettimeofday(&tp, NULL);
    end = tp.tv_usec;
    cout << "Write: " << end-start << endl;
    cout << endl;

    // Write to streaming image
    if ( tick % T_LATEST == 0) {
        fc.Convert(image, fp.img_ptr);
        last_img = Mat(fp.img_ptr->GetHeight(), fp.img_ptr->GetWidth(), CV_8UC3,
                       (uint8_t *) image.GetBuffer());

        thread t(&AgriDataCamera::writeLatestImage, this, last_img,
                 ref(compression_params));
        t.detach();
    }

    // Check Luminance (and add to documents)
    if ( tick % T_LUMINANCE == 0) {

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
    if ( tick % T_MONGODB  == 0 ) {
        gettimeofday(&tp, NULL);
        start= tp.tv_usec;
        cout << "Dumping " << documents.size() <<  " documents";
        frames.insert_many(documents);
        documents.clear();

        gettimeofday(&tp, NULL);
        end = tp.tv_usec;
        cout << "Document insertion: " << end-start << endl;
    }
}

/**
 * Rotation
 * 
 * Rotate the input image by an angle (the proper way!)
 */

Mat AgriDataCamera::Rotate(Mat input, double angle) {
    // Get rotation matrix for rotating the image around its center
    cv::Point2f center(input.cols/2.0, input.rows/2.0);
    cv::Mat rot = cv::getRotationMatrix2D(center, angle, 1.0);
    
    // Determine bounding rectangle
    cv::Rect bbox = cv::RotatedRect(center, input.size(), angle).boundingRect();
    
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
void AgriDataCamera::Luminance(bsoncxx::oid id, cv::Mat input)
{
    struct timeval tp;
    long int start, end;
    gettimeofday(&tp, NULL);
    start= tp.tv_usec;

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
    cout << "Modifying id: " << id.to_string() << endl;
    _frames.update_one(bsoncxx::builder::stream::document {} << "_id" << id << bsoncxx::builder::stream::finalize,
                       bsoncxx::builder::stream::document {} << "$set" << bsoncxx::builder::stream::open_document <<
                       "luminance" << avgLum << bsoncxx::builder::stream::close_document << bsoncxx::builder::stream::finalize);

    gettimeofday(&tp, NULL);
    end = tp.tv_usec;
    cout << "Luminance: " << end-start << endl;
    cout << "Luminance Data: " << avgLum << endl;
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

void AgriDataCamera::Snap()
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

        Mat last_img;
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
void AgriDataCamera::writeLatestImage(Mat img, vector<int> compression_params)
{
    string snumber;
    snumber = DeviceID();

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
int AgriDataCamera::Stop()
{

    syslog(LOG_INFO, "Recording Stopped");
    isRecording = false;

    cout << "Dumping documents";
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
json AgriDataCamera::GetStatus()
{
    json status;
    json imu_status;
    imu_status["IMU_VELOCITY_NORTH"] = -99.9;
    imu_status["IMU_VELOCITY_EAST"] = -99.9;

    // If we are not recording, make a new IMU request
    // If we are recording, this can get in the way of the frame grabber's communication with the imu
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

    status["Serial Number"] = (string) DeviceID();
    status["Model Name"] = (string) GetDeviceInfo().GetModelName();
    status["Recording"] = isRecording;

    // Something funny here, occasionally the ptrGrabResult is not available
    // even though the camera is grabbing?
    if (isRecording)
        status["Timestamp"] = last_timestamp;
    else
        status["Timestamp"] = "Not Recording";
    status["Exposure Time"] = ExposureTimeAbs();
    status["Resulting Frame Rate"] = ResultingFrameRateAbs();
    status["Current Gain"] = GainRaw();
    status["Temperature"] = TemperatureAbs();
    try {
        status["scanid"] = scanid;
    } catch (...) {
        status["scanid"] = "";
    }
    return status;
}
