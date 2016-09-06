// ZMQCPPTEST
//
// Created by Selwyn-Lloyd on 9/5/16.
// Copyright © 2016 AgriData. All rights reserved.
//

// Include files to use the PYLON API.
#include <pylon/PylonIncludes.h>
#include <pylon/usb/BaslerUsbInstantCamera.h>

// Include files to use openCV.
#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

// GenApi
#include <GenApi/GenApi.h>

// Utilities
#include "spdlog/spdlog.h"
#include "zmq.hpp"

// Additional include files.
#include <atomic>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

using namespace Basler_UsbCameraParams;
using namespace Pylon;
using namespace cv;
using namespace std;

atomic<bool> IsRecording(false);
int cFramesPerSecond;
string save_path_ori;

// convers pylon video stream into CPylonImage object
CPylonImage image;
// define 'pixel' output format (to match algorithm optimalization).
// PixelType_BGR8packed = BGR32 = CV_32FC4
CImageFormatConverter fc;
// This smart pointer will receive the grab result data. (Pylon).
CGrabResultPtr ptrGrabResult;

int initialize(CBaslerUsbInstantCamera& camera)
{
    system("CLS");
    
    // Create directory structure
    int status = mkdir("output", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    
    stringstream ss;
    time_t result = time(NULL);
    ss.str(asctime(localtime(&result)));
    string tm = ss.str();
    tm.resize(tm.size() - 1);
    
    // default hard coded settings if Config.cfg file is not present or
    // in-complete/commented
    bool show_screen_info = 1;
    bool save_video = 0;
    bool save_radius = 0;
    bool save_original = 1;
    double threshold_ = 20;
    int thres_mode = 0;
    int thres_type = 0;
    string thres_name = "THRESH_BINARY";
    int cFramesPerSecond = 20;
    int streaming_factor = cFramesPerSecond;
    string save_path = "/home/agridata/output/roi.avi";
    string save_path_num = "/home/agridata/output/num.txt";
    string save_path_ori = "/home/agridata/output/" + tm + ".avi";
    string stream_path = "/home/agridata/output/stream.jpg";
    Size ROI_dimensions = Size(1280, 1024);
    bool ROI_start_auto = 1;
    Size ROI_start;
    int ROI_start_x;
    int ROI_start_y;
    int width_SE = 3;
    int heigth_SE = 3;
    Size SE_morph = Size(width_SE, heigth_SE);
    int heigth_blur = 3;
    int width_blur = 3;
    Size blur_dimensions = Size(width_SE, heigth_SE);
    int itterations_close = 3;
    int threshold1_canny = 100;
    int threshold2_canny = 50;
    int aperture_canny = 3;
    double pupil_aspect_ratio = 1.5;
    int pupil_min = 15;
    int pupil_max = 55;
    bool original_image = 0;
    bool blurred_image = 0;
    bool thresholded_image = 0;
    bool closed_image = 0;
    bool canny_image = 0;
    bool end_result_image = 0;
    bool show_ost = 0;
    double size_text = 0.5;
    uint32_t time;
        
    // images for processing
    Mat eye;
    Mat thres;
    Mat roi_eye_rgb;
    Mat close;
    Mat blur;
    Mat canny;
    Mat roi_eye;
    Mat frame;
    
    // variables for numerical output
    ofstream output_end_result;
    ostringstream strs, ost1, ost2, ost3;
    string radius, size_roi, frame_rate, output_file;
	
    // -----------------------//
    // Show loaded variables //
    // -----------------------//
    
    if(show_screen_info == true) {
        
        cout << endl << endl;
        cout << "*------------------------------------------------------*" << endl;
        cout << "*****               Program Parameters             *****" << endl;
        cout << "*------------------------------------------------------*" << endl;
        cout << "*" << endl;
        cout << "* Save video output                   : " << save_video << endl;
        cout << "* Save original stream                : " << save_original << endl;
        cout << "* Save radius numerical output        : " << save_radius << endl;
        cout << "* Save path video output              : " << save_path << endl;
        cout << "* Save path original stream           : " << save_path_ori << endl;
        cout << "* Save path numerical output          : " << save_path_num << endl;
        cout << endl;
        cout << "* Frames per second                   : " << cFramesPerSecond << endl;
        cout << "* Heigth and width ROI                : " << ROI_dimensions << endl;
        if(ROI_start_auto == false) {
            cout << "* Anchor coordinate [X,Y] ROI n  manually set                  "
            "      : "
            << ROI_start << endl;
            } else {
            cout << "* Anchor coordinate [X,Y] ROI set automatically" << endl;
        }
        cout << endl;
        cout << "* Value of threshold                  : " << threshold_ << endl;
        cout << "* Threshold mode                      : (" << thres_mode << ") " << thres_name << endl;
        cout << "* Size Gaussian blur filter           : " << blur_dimensions << endl;
        cout << "* Size structuring element n  for morphological closing          "
        " : "
        << SE_morph << endl;
        cout << "* Total itterations closing operation : " << itterations_close << endl;
        cout << "* First threshold canny filter        : " << threshold1_canny << endl;
        cout << "* Second threshold canny filter       : " << threshold2_canny << endl;
        cout << "* Size aperture kernel canny filter   : " << aperture_canny << endl;
        cout << "* Threshold aspect ratio ellipse      : " << pupil_aspect_ratio << endl;
        cout << "* Minimum radius accepted ellipse     : " << pupil_min << endl;
        cout << "* Maximum radius accepted ellipse     : " << pupil_max << endl;
        cout << endl;
        cout << "* Show original stream on display     : " << original_image << endl;
        cout << "* Show blurred stream on display      : " << blurred_image << endl;
        cout << "* Show thresholded stream on display  : " << thresholded_image << endl;
        cout << "* Show morph closed stream on display : " << closed_image << endl;
        cout << "* Show Canny filter stream on display : " << canny_image << endl;
        cout << "* Show end result stream on display   : " << end_result_image << endl;
        cout << endl;
        cout << "* Show text on end result stream      : " << show_ost << endl;
        cout << "* Size text on screen                 : " << size_text << endl;
        cout << "*" << endl;
        cout << "*------------------------------------------------------*" << endl;
        cout << "*******     Ready to start qcquisition. . .     ******** " << endl;
        cout << "*------------------------------------------------------*" << endl;
    }
    
    return 0;
	
}

int run(CBaslerUsbInstantCamera& camera)
{
	// Configuration / Initialization
	int exp_counter = 100;
    int wb_counter = 100;
    int stream_counter = 100;
	int frames = 999999999 * cFramesPerSecond;
	
	VideoWriter original;
	
    // Create an instant camera object with the camera device found first.
    // cameraObj camera(CTlFactory::GetInstance().CreateFirstDevice());
    
    // Print the model name of the camera.
    cout << endl << endl << "Connected Basler USB 3.0 device, type : " << camera.GetDeviceInfo().GetModelName() << endl;
    
    // open camera object to parse frame# etc.
    camera.Open();
    
    // Start the grabbing of c_countOfImagesToGrab images.
    // The camera device is parameterized with a default configuration
    // which
    // sets up free-running continuous acquisition.
    camera.StartGrabbing(frames);
    
    // Enable the acquisition frame rate parameter and set the frame rate.
    camera.AcquisitionFrameRateEnable.SetValue(true);
    camera.AcquisitionFrameRate.SetValue(cFramesPerSecond);
    
    // Get native width and height from connected camera
    GenApi::CIntegerPtr width(camera.GetNodeMap().GetNode("Width"));
    GenApi::CIntegerPtr height(camera.GetNodeMap().GetNode("Height"));
	
	// Minimize Exposure Time
	camera.AutoFunctionProfile_MinimizeExposureTimeEnable.SetValue(true);
	camera.AutoFunctionProfile_MinimizeExposureTime.SetVelue(true);
	
	// Continuous Auto Gain
	camera.GainAutoEnable.setValue(true);
	camera.GainAuto.setValue(GainAuto_Continuous);
    
    // The parameter MaxNumBuffer can be used to control the count of buffers
    // allocated for grabbing. The default value of this parameter is 10.
    camera.MaxNumBuffer = 10;
    
    // create Mat image template
    Mat cv_img(width->GetValue(), height->GetValue(), CV_8UC3);
    
    // open video file
    cout << Size(width->GetValue(), height->GetValue()) << endl;
    original.open(save_path_ori.c_str(), CV_FOURCC('M', 'P', 'E', 'G'), cFramesPerSecond,
    Size(width->GetValue(), height->GetValue()), true);
    
    // if the VideoWriter file is not initialized successfully, exit the
    // program.
    if(!original.isOpened()) {
        cout << "ERROR: Failed to write the video (ORIGINAL)" << endl;
        return -1;
    }
    
    // initiate main loop with algorithm
    while(IsRecording) {
        
        try {
            
            // Wait for an image and then retrieve it. A timeout of 5000
            // ms is used.
            camera.RetrieveResult(5000, ptrGrabResult, TimeoutHandling_ThrowException);
			fc.OutputPixelFormat = PixelType_Mono8;
            
            // Image grabbed successfully?
            if(ptrGrabResult->GrabSucceeded()) {
                
                // convert to Mat - openCV format for analysis
                fc.Convert(image, ptrGrabResult);
                cv_img = Mat(ptrGrabResult->GetHeight(), ptrGrabResult->GetWidth(), CV_8U, (uint8_t*)image.GetBuffer());
                
                // write the original stream into file
                // conversion of Mat file is necessary prior to saving
                // with mpeg
                // compression
                cvtColor(cv_img, cv_img, CV_GRAY2RGB);
                
                // write to streaming jpeg
                if(stream_counter == 0) {
                    imwrite("/home/agridata/Desktop/embeddedServer/EmbeddedServer/images/streaming.png", cv_img);
                    stream_counter = 0;
				} else {
                    stream_counter++;
                }
                
				/*
                // Auto White Balance
                if(wb_counter == 0) {
                    wb_counter = 100;
                    cout << "Changing white balance: " << camera.BalanceRatio.GetValue() << endl;
                    // camera.BalanceWhiteAutoEnable.SetValue(true);
                    camera.BalanceWhiteAuto.SetValue(BalanceWhiteAuto_Once);
				} else {
                    wb_counter--;
                }
                
                // Auto Exposure
                if(exp_counter == 0) {
                    cout << "Changing exposure: " << camera.ExposureTime.GetValue() << endl;
                    exp_counter = 100;
                    // camera.ExposureAutoEnable.setValue(true);
                    camera.ExposureAuto.SetValue(ExposureAuto_Once);
				} else {
                    exp_counter--;
                }
				 */
            }
            
		} catch (const GenericException &e) {
			cout << "Error: " << ptrGrabResult->GetErrorCode() << " ";
			cout << ptrGrabResult->GetErrorDescription() << endl;
        }
    }
    
    return 0;
}

int stop(CBaslerUsbInstantCamera &camera) {
	camera.StopGrabbing();
	IsRecording = false;

	cout << endl << endl << " *** Done ***" << endl << endl;

	// Releases all pylon resources.
	PylonTerminate();

	return 0;
}

int snap(CBaslerUsbInstantCamera &camera) {
    // Wait for an image and then retrieve it. A timeout of 5000 ms is used.
     camera.RetrieveResult(5000, ptrGrabResult, TimeoutHandling_ThrowException);
	
	fc.OutputPixelFormat = PixelType_Mono8;
    
    // Image grabbed successfully?
    if(ptrGrabResult->GrabSucceeded()) {
        
        // convert to Mat - openCV format for analysis
        fc.Convert(image, ptrGrabResult);
        Mat cv_img = Mat(ptrGrabResult->GetHeight(), ptrGrabResult->GetWidth(), CV_8U, (uint8_t*)image.GetBuffer());
        imwrite("/home/agridata/Desktop/embeddedServer/EmbeddedServer/images/", cv_img);
        } else {
        cout << "Error: " << ptrGrabResult->GetErrorCode() << " ";
        cout << ptrGrabResult->GetErrorDescription() << endl;
    }
    return 0;
}

void gowhile() {
	while(IsRecording) {
		cout << rand();
		usleep(2000);
	}
}

void stopwhile() {
	IsRecording = false;
}

int main()
{
    
    zmq::context_t context(1);
    zmq::socket_t server(context, ZMQ_REP);
    server.bind("tcp://*:4999");
    
    zmq_sleep(1.5); // Wait for sockets
	
	PylonInitialize();
	
	IsRecording = true; /* Remove this */
	
	//CBaslerUsbInstantCamera camera(CTlFactory::GetInstance().CreateFirstDevice());
	int ret;
	// ret = initialize(camera);
	thread t (gowhile);
	
	
    
    while(true) {
        
        zmq::message_t messageR;
        
        server.recv(&messageR);
        
        std::string recieved = std::string(static_cast<char*>(messageR.data()), messageR.size());
        
        printf("%sn", recieved.c_str());
		
		//Parse the string
		char** argv;
		int argc = 0;
		size_t pos = 0;
		string s = received;
		string token;
		string delimiter = "_";
		while ((pos = s.find(delimiter)) != string::npos) {
			token = s.substr(0, pos);
			argv[argc];
			argc++;
			s.erase(0, pos + delimiter.length());
		}
		
		// Choose action
		if ( argv[0] == "stop" ) {
			stop();
			break;
		} else if ( argv[0] == "start") {
			start();
			break;
		} else if ( argv[0] == "BalanceWhite") {
			camera.BalanceWhiteAutoEnable.SetValue(true);
			camera.BalanceWhiteAuto.SetValue(arg[1]);
			break;
		} else if ( argv[0] == "ExposureBalance") {
			camera.ExposureAuto.SetValueEnable(true);
			camera.ExposureAuto.SetValue(arg[1]);
		} else if ( argv[0] == "AutoExposureTimeUpperLimit" ) {
			camera.AutoExposureTimeUpperLimitEnable.SetValue(true);
			camera.AutoExposureTimeUpperLimit.SetValue(atoi(argv[1].c_str()));
		} else if ( argv[0] == "AutoTargetBrightness" ) {
			camera.AutoTargetBrightnessEnable.SetValue(true);
			camera.AutoTargetBrightness.SetValue(atoi(argv[1].c_str()));
		} else if ( argv[0] == "AutoGainUpperLimit") {
			camera.GainSelector.SetValue(GainSelector_All);
			camera.AutoGainUpperLimit.SetValue(atof(argv[1].c_str()));
		} else if ( argv[0] == "AutoGainLowerLimit") {
			camera.GainSelector.SetValue(GainSelector_All);
			camera.AutoGainLowerLimit.SetValue(atof(argv[1].c_str()));
		}
			
		//ret = run(camera);
		stopwhile();
		// or perhaps delete the thread?
        
        std::string reply = "This is a message from the c++ code";
        zmq::message_t messageS(reply.size());
        memcpy(messageS.data(), reply.data(), reply.size());
        server.send(messageS);
    }
    
    return 0;
}
