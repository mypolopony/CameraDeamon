/* 
 * File:   AgriDataCamera.h
 * Author: agridata
 *
 * Created on March 13, 2017, 1:33 PM
 */

#ifndef AGRIDATACAMERA_H
#define AGRIDATACAMERA_H

// Include files to use the PYLON API.
#include <pylon/PylonIncludes.h>
#include <pylon/usb/BaslerUsbInstantCamera.h>
#include <pylon/usb/BaslerUsbInstantCameraArray.h>
#include <pylon/usb/_BaslerUsbCameraParams.h>

// GenApi
#include <GenApi/GenApi.h>

// Include files to use openCV.
#include "opencv2/core.hpp"


class AgriDataCamera : public Pylon::CBaslerUsbInstantCamera {
public:
    AgriDataCamera();
    AgriDataCamera(const AgriDataCamera& orig);
	
	void Initialize();
	void Run();
	void Stop();
	std::string GetStatus();
	
    virtual ~AgriDataCamera();
	
	std::string scanid;
	
private:
    // Variables (TODO: Move to config file)
    int frames_per_second = 20;
    int exposure_lower_limit = 61;
    int exposure_upper_limit = 1200;
	bool isRecording = false;
	
	static void writeHeaders(std::ofstream &fout);
	void writeFrameLog(std::ofstream &fout, uint64_t camtime);
	void writeLatestImage(cv::Mat cv_img, std::vector<int> compression_params);
	
};

#endif /* AGRIDATACAMERA_H */

