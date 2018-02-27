/*
 * File:   AgriDataCamera.h
 * Author: agridata
 *
 * Created on March 13, 2017, 1:33 PM
 */

#ifndef AGRIDATACAMERA_H
#define AGRIDATACAMERA_H

#include <fstream>

// Include files to use the PYLON API.
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
#include "hdf5.h"
#include "hdf5_hl.h"

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



class AgriDataCamera : public Pylon::CInstantCamera
{
public:
    AgriDataCamera();

    void Initialize();
    void Run();
    int Stop();
    void Snap();
    nlohmann::json GetStatus();

    virtual ~AgriDataCamera();

    std::string scanid;
    bool isPaused;
    bool isRecording;
    std::string serialnumber;
    GenApi::INodeMap &nodeMap;

private:
    struct FramePacket {
        std::string camera_time, time_now, imu_data;
        float balance_red, balance_green, balance_blue, exposure_time;
        Pylon::CGrabResultPtr img_ptr;
    };

    // Dimensions
    int64_t width;
    int64_t height;

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

    // Videowriter and filename
    cv::VideoWriter videowriter;
    std::string save_prefix;
    std::string videofile;

    // Frame log stream
    std::ofstream frameout;

    // PNG compression
    std::vector<int> compression_params;

    // Camera rotation
    int rotation;

    // Timers
    const int T_LATEST = 20;            // Every second
    const int T_MONGODB = 60*20;        // Every minute
    const int T_LUMINANCE = 10;         // Every half second
    uint8_t tick;                       // Running counter

    // Output Parameters
    uint8_t max_filesize = 3;
    std::string output_prefix;
    std::string output_dir;

    // HDF5
    hid_t hdf5_output;
    std::string current_hdf5_file;

    // MongoDB
    std::string MONGODB_HOST = "mongodb://localhost:27017";
    mongocxx::client conn;
    mongocxx::database db;
    mongocxx::collection frames;
    std::vector<bsoncxx::v_noabi::document::value> documents;

    // Timestamp (should go in status block)
    std::string last_timestamp;

    // IMU_Data (to store while recording)
    std::string last_imu_data;

    // ZMQ
    zmq::context_t ctx_;
    zmq::socket_t imu_;

    // Methods
    void Luminance(bsoncxx::oid, cv::Mat);
    cv::Mat Rotate(cv::Mat);
    void writeHeaders();
    void HandleFrame(AgriDataCamera::FramePacket);
    void writeLatestImage(cv::Mat, std::vector<int>);
    std::string imu_wrapper(AgriDataCamera::FramePacket);
};

#endif /* AGRIDATACAMERA_H */
