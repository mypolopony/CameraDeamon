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
#include <pylon/usb/BaslerUsbInstantCamera.h>
#include <pylon/usb/BaslerUsbInstantCameraArray.h>
#include <pylon/usb/_BaslerUsbCameraParams.h>

// GenApi
#include <GenApi/GenApi.h>

// Include files to use openCV.
#include "opencv2/core.hpp"
#include "opencv2/highgui.hpp"

// Utilities
#include "json.hpp"


class AgriDataCamera : public Pylon::CBaslerUsbInstantCamera {
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

private:
    // Dimensions
    int64_t width;
    int64_t height;

    // Mat image templates
    cv::Mat cv_img;
    cv::Mat last_img;

    // This smart pointer will receive the grab result data
    Pylon::CGrabResultPtr ptrGrabResult;

    // CPylonImage object as a destination for reformatted image stream
    Pylon::CPylonImage image;

    // Image converter
    Pylon::CImageFormatConverter fc;

    // Videowriter and filename
    cv::VideoWriter videowriter;
    std::string save_prefix;

    // Frame log stream
    std::ofstream frameout;

    // PNG compression
    std::vector<int> compression_params;

    // Timers
    uint8_t latest_timer;
    uint8_t filesize_timer;

    // Output Parameters
    uint8_t max_filesize = 3;
    std::string output_prefix;
    std::string output_dir;

    // Methods
    void writeHeaders();
    void HandleFrame(Pylon::CGrabResultPtr ptrGrabResult);
    void writeLatestImage(cv::Mat img, std::vector<int> compression_params);

};

#endif /* AGRIDATACAMERA_H */

