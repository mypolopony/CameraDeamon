// ZMQCPPTEST
//
// Created by Selwyn-Lloyd on 9/5/16.
// Copyright © 2016 AgriData. All rights reserved.
//

// Include files to use the PYLON API.
#include <pylon/PylonIncludes.h>
#include <pylon/usb/BaslerUsbInstantCamera.h>
#include <pylon/usb/_BaslerUsbCameraParams.h>

// Include files to use openCV.
#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

// GenApi
#include <GenApi/GenApi.h>

// Utilities
#include "zmq.hpp"

// Additional include files.
#include <atomic>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <exception>
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
    // Create directory structure
    int status = mkdir("output", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    
    stringstream ss;
    time_t result = time(NULL);
    ss.str(asctime(localtime(&result)));
    string tm = ss.str();
    tm.resize(tm.size() - 1);
    save_path_ori = "/home/agridata/output/" + tm + ".avi";

    // FPS
    cFramesPerSecond = 180 ;
    
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
    int streaming_factor = cFramesPerSecond;
    string save_path = "/home/agridata/output/roi.avi";
    string save_path_num = "/home/agridata/output/num.txt";
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

void run(CBaslerUsbInstantCamera& camera)
{
    // Configuration / Initialization
    int heartbeat = 100;
    int stream_counter = 200;
    int frames = 999 * cFramesPerSecond;
    IsRecording = true;
    struct stat filestatus;
    
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
    
    // Continuous Auto Gain
    //camera.GainAutoEnable.SetValue(true);
    camera.GainAuto.SetValue(GainAuto_Continuous);
    
    // create Mat image template
    Mat cv_img(width->GetValue(), height->GetValue(), CV_8UC3);
    
    // open video file
    cout << Size(width->GetValue(), height->GetValue()) << endl;
    original.open(save_path_ori.c_str(), CV_FOURCC('M', 'P', 'E', 'G'), cFramesPerSecond, Size(width->GetValue(), height->GetValue()), true);
    
    // Streaming image compression
    vector<int> compression_params;
    compression_params.push_back(CV_IMWRITE_PNG_COMPRESSION);
    compression_params.push_back(3);

    // if the VideoWriter file is not initialized successfully, exit the
    // program.
    if(!original.isOpened()) {
        cout << "ERROR: Failed to write the video (ORIGINAL)" << endl;
    }
    
    // initiate main loop with algorithm
    while(IsRecording) {
        
        try {
            
            // Wait for an image and then retrieve it. A timeout of 5000
            // ms is used.
            camera.RetrieveResult(5000, ptrGrabResult, TimeoutHandling_ThrowException);
            fc.OutputPixelFormat = PixelType_BGR8packed;
            
            // Image grabbed successfully?
            if(ptrGrabResult->GrabSucceeded()) {
                
                // convert to Mat - openCV format for analysis
                fc.Convert(image, ptrGrabResult);
                cv_img = Mat(ptrGrabResult->GetHeight(), ptrGrabResult->GetWidth(), CV_8UC3, (uint8_t*)image.GetBuffer());
                
                // write the original stream into file
                //cvtColor(cv_img, cv_img, CV_GRAY2BGR);
                original.write(cv_img);
				
                // write to streaming jpeg
                if(stream_counter == 0) {
                    imwrite("/home/agridata/Desktop/embeddedServer/EmbeddedServer/images/streaming.png", cv_img, compression_params);
                    stream_counter = 200;
                } else {
                    stream_counter--;
                }

                // Check file size
                if (heartbeat == 0) {
                    stat(save_path_ori.c_str(), &filestatus);
                    int size = filestatus.st_size;
                    if (size > 1073741824) {            // 1GB = 1073741824 bytes
					stringstream ss;
                        time_t result = time(NULL);
                        ss.str(asctime(localtime(&result)));
                        string tm = ss.str();
                        tm.resize(tm.size() - 1);
                        save_path_ori = "/home/agridata/output/" + tm + ".avi";
    
                        original.open(save_path_ori.c_str(), CV_FOURCC('M', 'P', 'E', 'G'), cFramesPerSecond, Size(width->GetValue(), height->GetValue()), true);

                    }
                    heartbeat = 500;   
                } else {
                    heartbeat--;
                }
                
                /*
                // Auto White Balance
                if(heartbeat == 0) {
                    heartbeat = 100;
                    cout << "Changing white balance: " << camera.BalanceRatio.GetValue() << endl;
                    // camera.BalanceWhiteAutoEnable.SetValue(true);
                    camera.BalanceWhiteAuto.SetValue(BalanceWhiteAuto_Once);
                } else {
                    heartbeat--;
                }
                
                // Auto Exposure
                if(heartbeat == 0) {
                    cout << "Changing exposure: " << camera.ExposureTime.GetValue() << endl;
                    heartbeat = 100;
                    // camera.ExposureAutoEnable.setValue(true);
                    camera.ExposureAuto.SetValue(ExposureAuto_Once);
                } else {
                    heartbeat--;
                }
                 */
            }
            
        } catch (const GenericException &e) {
            cout << "Error: " << ptrGrabResult->GetErrorCode() << " ";
            cout << ptrGrabResult->GetErrorDescription() << endl;
        }
    }
    camera.StopGrabbing();
}

int stop(CBaslerUsbInstantCamera &camera) {
    IsRecording = false;
    cout << endl << endl << " *** Done ***" << endl << endl;

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
        imshow("original",cv_img);
    } else {
        cout << "Error: " << ptrGrabResult->GetErrorCode() << " ";
        cout << ptrGrabResult->GetErrorDescription() << endl;
    }
    return 0;
}

vector<string> split(const string &s, char delim) {
    stringstream ss(s);
    string item;
    vector <string>tokens;
    while (getline(ss, item, delim)) {
        tokens.push_back(item);
    }
    return tokens;
}

int main()
{
    PylonInitialize();
    
    // Listen on port 4999
    zmq::context_t context(1);
    zmq::socket_t client(context, ZMQ_SUB);
    client.connect("tcp://127.0.0.1:4999");
    
    // Publish on 4448
    zmq::socket_t publisher(context, ZMQ_PUB);
    publisher.bind("tcp://*:4998");

    
    zmq_sleep(1.5); // Wait for sockets
    
    bool block = false;
    int ret;
    CBaslerUsbInstantCamera camera(CTlFactory::GetInstance().CreateFirstDevice());
    
    while(true) {
        
        zmq::message_t messageR;
        
        client.recv(&messageR, ZMQ_NOBLOCK);
        
        string recieved = string(static_cast<char*>(messageR.data()), messageR.size());
        
        printf("%sn", recieved.c_str());
        
        //Parse the string
        char** argv;
        int argc = 0;
        size_t pos = 0;
        string s;
        char delimiter = '_';
        string reply;

        s = recieved;
        vector <string>tokens = split(s,delimiter);
		string id_hash;
		
		ostringstream oss;
		
        if (!block) {

            try {
				
                oss << id_hash << "_1";        // Innocent until proven guilty
                reply = oss.str();
				
				id_hash = tokens[0];
				cout << id_hash;
				
				
                // Choose action
                if (tokens[1] == "start") {
                    if (IsRecording) {
                        oss << id_hash << "_0_NotRecording";
						reply = oss.str();
                    } else {
                        ret = initialize(ref(camera));
                        
                        thread t (run, ref(camera));
                        t.detach();
                    }
                } else if (tokens[1] == "stop") {
                    ret = stop(ref(camera));
                } else if (tokens[1] == "BalanceWhiteAuto") {
                    if (tokens[2] == "BalanceWhiteAuto_Once") {
                        camera.BalanceWhiteAuto.SetValue(BalanceWhiteAuto_Once);
                    } else if (tokens[2] == "BalanceWhiteAuto_Continuous") {
                        camera.BalanceWhiteAuto.SetValue(BalanceWhiteAuto_Continuous);
                    } else if (tokens[2] == "BalanceWhiteAuto_Off") {
                        camera.BalanceWhiteAuto.SetValue(BalanceWhiteAuto_Off);
                    }
                } else if (tokens[1] == "ExposureAuto") {
                    if (tokens[2] == "Once") {
                        camera.ExposureAuto.SetValue(ExposureAuto_Once);
                    }
                } else if (tokens[1] == "AutoFunctionProfile") {
                    if (tokens[2] == "AutoFunctionProfile_MinimizeExposure") {
                        camera.AutoFunctionProfile.SetValue(AutoFunctionProfile_MinimizeExposureTime);
                    } else if (tokens[2] == "AutoFunctionProfile_MinimizeGain") {
                        camera.AutoFunctionProfile.SetValue(AutoFunctionProfile_MinimizeGain);
                    }
                } else if (tokens[1] == "GainAuto") {
                    if (tokens[2] == "GainAuto_Once") {
                        camera.GainAuto.SetValue(GainAuto_Once);
                    } else if (tokens[2] == "GainAuto_Continuous") {
                        camera.GainAuto.SetValue(GainAuto_Continuous);
                    } else if (tokens[2] == "GainAuto_Off") {
                        camera.GainAuto.SetValue(GainAuto_Off);
                    }
                } else if (tokens[1] == "ExposureAuto") {
                    if (tokens[2] == "Exposure_Once") {
                        camera.ExposureAuto.SetValue(ExposureAuto_Once);
                    } else if (tokens[2] == "ExposureAuto_Continuous") {
                        camera.ExposureAuto.SetValue(ExposureAuto_Continuous);
                    } else if (tokens[2] == "ExposureAuto_Off") {
                        camera.ExposureAuto.SetValue(ExposureAuto_Off);
                    }
                } else if (tokens[1] == "BalanceRatioSelector") {
                    if (tokens[2] == "BalanceRatioSelector_Green") {
                        camera.BalanceRatioSelector.SetValue(BalanceRatioSelector_Green);
                    } else if (tokens[2] == "BalanceRatioSelector_Red") {
                        camera.BalanceRatioSelector.SetValue(BalanceRatioSelector_Red);
                    } else if (tokens[2] == "BalanceRatioSelector_Blue") {
                        camera.BalanceRatioSelector.SetValue(BalanceRatioSelector_Blue);
                    }
                } else if (tokens[1] == "GainSelector") {
                    camera.GainSelector.SetValue(GainSelector_All);
                } else if (tokens[1] == "Gain") {
                    camera.Gain.SetValue(atof(tokens[1].c_str()));
                } else if (tokens[1] == "BalanceRatio") {
                    camera.BalanceRatio.SetValue(atof(tokens[1].c_str()));
                } else if (tokens[1] == "AutoTargetBrightness") {
                    camera.AutoTargetBrightness.SetValue(atof(tokens[1].c_str()));
                } else if (tokens[1] == "AutoExposureTimeUpperLimit") {
                    camera.AutoExposureTimeUpperLimit.SetValue(atof(tokens[1].c_str()));
                } else if (tokens[1] == "AutoGainUpperLimit") {
                    camera.GainSelector.SetValue(GainSelector_All);     // Backup in case we forget
                    camera.AutoGainUpperLimit.SetValue(atof(tokens[1].c_str()));
                } else if (tokens[1] == "AutoGainLowerLimit") {         // Backup incase we forget
                    camera.GainSelector.SetValue(GainSelector_All);
                    camera.AutoGainLowerLimit.SetValue(atof(tokens[1].c_str()));
                } else if (tokens[1] == "GetStatus") {
                    oss << id_hash << "_"
						<< "Is recording: " << IsRecording << endl
                        << "White Balance Ratio: " << camera.BalanceRatio.GetValue() << endl
                        << "Auto FunctionProfile: " << camera.AutoFunctionProfile.GetValue() << endl
                        << "Balance Ratio Selector: " << camera.BalanceRatioSelector.GetValue() << endl
                        << "White Balance Auto: " << camera.BalanceWhiteAuto.GetValue() << endl
                        << "Auto Exposure: " << camera.ExposureAuto.GetValue() << endl
                        << "Exposure Mode: " << camera.ExposureMode.GetValue() << endl
                        << "Gain: " << camera.Gain.GetValue() << endl
                        << "Gain Auto: " << camera.GainAuto.GetValue() << endl
                        << "Framerate: " << camera.AcquisitionFrameRate.GetValue() << endl
                        << "Target Brightness: " << camera.AutoTargetBrightness.GetValue() << endl;
                    reply = oss.str();    
                } else {
                    oss << id_hash << "_0_CommandNotFound";
					reply = oss.str();
                }
            } catch (...) {
                oss << id_hash << "_0_ExceptionProcessingCommand";
				reply = oss.str();
            }
        } else {
            oss << id_hash << "_0_CameraIsBusy";
			reply = oss.str();
        }
		cout << reply;
        
        zmq::message_t messageS(reply.size());
        memcpy(messageS.data(), reply.data(), reply.size());
        publisher.send(messageS);
        block = false;
    }
    
    return 0;
}

