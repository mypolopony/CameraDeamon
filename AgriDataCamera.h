/*
 * File:   AgriDataCamera.h
   Author: agridata
 */

#ifndef AGRIDATACAMERA_H
#define AGRIDATACAMERA_H

// Standard
#include <fstream>

// Pylon
#include <pylon/PylonIncludes.h>
#include <pylon/InstantCamera.h>
#include <pylon/gige/BaslerGigEInstantCamera.h>
#include <pylon/gige/BaslerGigEInstantCameraArray.h>
#include <pylon/gige/_BaslerGigECameraParams.h>
#include <pylon/usb/BaslerUsbInstantCamera.h>
#include <pylon/usb/BaslerUsbInstantCameraArray.h>
#include <pylon/usb/_BaslerUsbCameraParams.h>

// GenApi
#include <GenApi/GenApi.h>

// Include files to use openCV.
#include "opencv2/core.hpp"
#include "opencv2/highgui.hpp"

// HDF5
#include "H5Cpp.h"

// Boost
#include <boost/optional/optional_io.hpp>

// Utilities
#include "json.hpp"
#include "zmq.hpp"
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/types.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>

// This is discouraged, as it brings the namespace in the global scope
// but for some reason, there is perhaps a name conflict, without it,
// i.e. just using void Oneshot(nlohmann::json) does not work:
//      ../lib/json.hpp:870:9: error: static assertion failed: could not find from_json() method in T's namespace
// But there is only ony cpp file and it also explicitly uses this namespace
// as well, so it should be OK.
// using json = nlohmann::json;


class AgriDataCamera : public Pylon::CBaslerGigEInstantCamera
{

public:
    AgriDataCamera();

    void Initialize();
    void Start(nlohmann::json);
    int Stop();
    void snapCycle();
    float _luminance(cv::Mat);
    nlohmann::json GetStatus();

    virtual ~AgriDataCamera();

    std::string scanid;
    std::string session_name;
    bool isRecording;
    bool calibration;
    std::string serialnumber;
    std::string modelname;
    std::string COLOR_FMT;

private:
    struct FramePacket {
        int64_t time_now;
        float exposure_time;
        nlohmann::json task;
        Pylon::CGrabResultPtr img_ptr;
    };

    // Dimensions (change these to ALL CAPS?)
    int64_t width;
    int64_t height;

    // Target dimensions
    int TARGET_HEIGHT = 960;
    int TARGET_WIDTH  = 600;

    // Rotation
    std::string rotation;

    // Mat image templates
    cv::Mat cv_img;
    cv::Mat last_img;
    cv::Mat small_last_img;

    // This smart pointer will receive the grab result data
    Pylon::CGrabResultPtr ptrGrabResult;

    // CPylonImage object as a destination for reformatted image stream
    Pylon::CPylonImage image;

    // Image converter
    Pylon::CImageFormatConverter fc;

    // Output base
    std::string save_prefix;

    // Image compression
    std::vector<int> compression_params;

    // Video output
    cv::VideoWriter oVideoWriter;

    // Timers
    const int T_LATEST = 20;
    const int T_MONGODB = 400; 
    const int T_LUMINANCE = 10;
    const int T_SAMPLE = 10;
    int T_CALIBRATION = 0;              // First five minutes are calibration (0 is disabled)
    int tick;                           // Running counter

    // Dynamic Framerate
    double HIGH_FPS;                    // To be set on initialization (cannot be const)
    const double LOW_FPS = 5;           // Probationary frame rate
    int RT_PROBATION = -1;              // Restricted period (this is a reverse timer, -1 is safe)
    const int PROBATION = 50;           // Counts down

    // Output Parameters
    uint8_t max_filesize = 3;
    std::string output_prefix;
    std::string output_dir;

    // HDF5
    hid_t hdf5_out, dataSetId, dataSpaceId, memSpaceId, vlDataTypeId, dataTypeId, pListId;
    std::string current_hdf5_file;

    // MongoDB
    std::string MONGODB_HOST = "mongodb://localhost:27017";
    mongocxx::client conn;
    mongocxx::database db;
    mongocxx::collection frames;
    std::vector<bsoncxx::v_noabi::document::value> documents;

    // Timestamp (should go in status block)
    int64_t last_timestamp;

    // Frame number (will monotonically increase)
    int frame_number;

    // ZMQ
    zmq::context_t ctx_;
    zmq::socket_t * s_client_socket (zmq::context_t & context);

    // Client info
    std::string clientid;

    // Methods
    std::string padTo(int, size_t);
    int GetFrameNumber(std::string);
    void Luminance(bsoncxx::oid, cv::Mat);
    void writeHeaders();
    void HandleFrame(AgriDataCamera::FramePacket);
    void HandleOneFrame(AgriDataCamera::FramePacket);
    void writeLatestImage(cv::Mat, std::vector<int>);
    void AddTask(std::string);
};

#endif /* AGRIDATACAMERA_H */