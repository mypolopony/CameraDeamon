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

// Utilities
#include "json.hpp"
#include "zmq.hpp"
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>


class AgriDataCamera : public Pylon::CBaslerGigEInstantCamera
{
public:
    AgriDataCamera();

    void Initialize();
    void Run();
    int Stop();
    void Snap();
    float _luminance(cv::Mat);
    nlohmann::json GetStatus();

    virtual ~AgriDataCamera();

    std::string scanid;
    std::string session_name;
    bool isPaused;
    bool isRecording;
    bool calibration;
    std::string serialnumber;
    std::string modelname;

private:
    struct FramePacket {
        int64_t time_now;
        float exposure_time;
        Pylon::CGrabResultPtr img_ptr;
    };

    // Dimensions (change these to ALL CAPS?)
    int64_t width;
    int64_t height;

    // Target dimensions
    int TARGET_HEIGHT = 960;
    int TARGET_WIDTH  = 600;

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
    Pylon::CImagePersistenceOptions persistenceOptions;

    // Output base
    std::string save_prefix;

    // Image compression
    std::vector<int> compression_params;

    // Timers
    const int T_LATEST = 20;            // Every second
    const int T_MONGODB = 60*20;        // Every minute
    const int T_LUMINANCE = 10;         // Every half second
    const int T_SAMPLE = 10;		// Every half second
    int T_CALIBRATION = 0;              // First five minutes are calibration
    int tick;                           // Running counter

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

    // ZMQ
    zmq::context_t ctx_;
    zmq::socket_t * s_client_socket (zmq::context_t & context);

    // Client info
    std::string clientid;

    // Methods
    void Luminance(bsoncxx::oid, cv::Mat);
    void writeHeaders();
    void HandleFrame(AgriDataCamera::FramePacket);
    void writeLatestImage(cv::Mat, std::vector<int>);
    void AddTask(std::string);
};

#endif /* AGRIDATACAMERA_H */
