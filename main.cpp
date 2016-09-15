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
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>

using namespace Basler_UsbCameraParams;
using namespace Pylon;
using namespace cv;
using namespace std;

int cFramesPerSecond;
string save_path_ori;
string logfile;
double timelimit = 5.0;  // In Minutes

// convers pylon video stream into CPylonImage object
CPylonImage image;
// define 'pixel' output format (to match algorithm optimalization).
// PixelType_BGR8packed = BGR32 = CV_32FC4
CImageFormatConverter fc;
// This smart pointer will receive the grab result data. (Pylon).
CGrabResultPtr ptrGrabResult;

string RecordingFile = "/home/agridata/CameraDeamon/IsRecording";

inline bool IsRecording () {
  struct stat buffer;   
  return (stat (RecordingFile.c_str(), &buffer) == 0); 
}

void SetRecording(bool val) {
  if (val) {
    ofstream outfile (RecordingFile);
    outfile << "." << std::endl;
    outfile.close();
  }
  if (!val) {
    remove(RecordingFile.c_str());
  }
}

int initialize(CBaslerUsbInstantCamera &camera) {
  // Create directory structure
  int status = mkdir("output", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

  // FPS
  cFramesPerSecond = 20;

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
  Mat cv_img;

  // variables for numerical output
  ofstream output_end_result;
  ostringstream strs, ost1, ost2, ost3;
  string radius, size_roi, frame_rate, output_file;

  // -----------------------//
  // Show loaded variables //
  // -----------------------//

  if (show_screen_info == true) {
    cout << endl << endl;
    cout << "*------------------------------------------------------*" << endl;
    cout << "*****               Program Parameters             *****" << endl;
    cout << "*------------------------------------------------------*" << endl;
    cout << "*" << endl;
    cout << "* Save video output                   : " << save_video << endl;
    cout << "* Save original stream                : " << save_original << endl;
    cout << "* Save radius numerical output        : " << save_radius << endl;
    cout << "* Save path video output              : " << save_path << endl;
    cout << "* Save path numerical output          : " << save_path_num << endl;
    cout << endl;
    cout << "* Frames per second                   : " << cFramesPerSecond
         << endl;
    cout << "* Heigth and width ROI                : " << ROI_dimensions
         << endl;
    if (ROI_start_auto == false) {
      cout << "* Anchor coordinate [X,Y] ROI n  manually set                  "
              "      : "
           << ROI_start << endl;
    } else {
      cout << "* Anchor coordinate [X,Y] ROI set automatically" << endl;
    }
    cout << endl;
    cout << "* Value of threshold                  : " << threshold_ << endl;
    cout << "* Threshold mode                      : (" << thres_mode << ") "
         << thres_name << endl;
    cout << "* Size Gaussian blur filter           : " << blur_dimensions
         << endl;
    cout << "* Size structuring element n  for morphological closing          "
            " : "
         << SE_morph << endl;
    cout << "* Total itterations closing operation : " << itterations_close
         << endl;
    cout << "* First threshold canny filter        : " << threshold1_canny
         << endl;
    cout << "* Second threshold canny filter       : " << threshold2_canny
         << endl;
    cout << "* Size aperture kernel canny filter   : " << aperture_canny
         << endl;
    cout << "* Threshold aspect ratio ellipse      : " << pupil_aspect_ratio
         << endl;
    cout << "* Minimum radius accepted ellipse     : " << pupil_min << endl;
    cout << "* Maximum radius accepted ellipse     : " << pupil_max << endl;
    cout << endl;
    cout << "* Show original stream on display     : " << original_image
         << endl;
    cout << "* Show blurred stream on display      : " << blurred_image << endl;
    cout << "* Show thresholded stream on display  : " << thresholded_image
         << endl;
    cout << "* Show morph closed stream on display : " << closed_image << endl;
    cout << "* Show Canny filter stream on display : " << canny_image << endl;
    cout << "* Show end result stream on display   : " << end_result_image
         << endl;
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

void run(CBaslerUsbInstantCamera &camera) {
  // Configuration / Initialization
  int heartbeat = 200;
  int heartbeat_log = 0;
  int stream_counter = 200;
  int frames = 999 * cFramesPerSecond;
  SetRecording(true);
  ofstream fout;
  ostringstream oss;
  struct stat filestatus;
  
  // Timestamp
  /*
  stringstream ss;
  time_t result = time(NULL);
  ss.str(asctime(localtime(&result)));
  string tm = ss.str();
  tm.resize(tm.size() - 1);
  */
  
  time_t rawtime;
  struct tm * timeinfo;
  char buffer [80];

  time (&rawtime);
  timeinfo = localtime (&rawtime);

  strftime (buffer,80,"%a %h %e %H_%M_%S %Y",timeinfo);
  string timenow(buffer);
   
  save_path_ori = "/home/agridata/output/" + timenow + ".avi";
  string logfile = "/home/agridata/output/" + timenow + ".txt";
  
  // Open and write logfile headers
  fout.open(logfile.c_str(),ios::app);
  oss << "DeviceSerialNumber" << ","
	  << "BalanceRatio" << "," 
	  << "AutoFunctionProfile"  << "," 
      << "BalanceRatioSelector" << "," 
      << "BalanceWhiteAuto" << "," 
	  << "Black Level" << ","
	  << "Gamma" << ","
	  << "ExposureAuto" << "," 
	  << "ExposureMode" << "," 
	  << "ExposureTime" << ","
	  << "Gain" << "," 
	  << "GainAuto" << "," 
	  << "AcquisitionFrameRate" << "," 
	  << "AutoGainLowerLimit" << "," 
	  << "AutoGainUpperLimit" << "," 
	  << "AutoExposureTimeLowerLimit" << "," 
	  << "AutoExposureTimeUpperLimit" << "," 
	  << "AutoTargetBrightness"
	  << endl;
	fout << oss.str();
	oss.str("");

  // VideoWriter
  VideoWriter original;

  // Create an instant camera object with the camera device found first.
  // cameraObj camera(CTlFactory::GetInstance().CreateFirstDevice());

  // Print the model name of the camera.
  cout << endl
       << endl
       << "Connected Basler USB 3.0 device, type : "
       << camera.GetDeviceInfo().GetModelName() << endl;

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
  
  // Exposure time limits
  camera.AutoExposureTimeLowerLimit.SetValue(400);
  camera.AutoExposureTimeUpperLimit.SetValue(1200);
  
  // Minimize Exposure 
  camera.AutoFunctionProfile.SetValue(AutoFunctionProfile_MinimizeExposureTime);
  
  // Continuous Auto Gain
  // camera.GainAutoEnable.SetValue(true);
  camera.GainAuto.SetValue(GainAuto_Continuous);
  camera.ExposureAuto.SetValue(ExposureAuto_Continuous);
  
  // Get native width and height from connected camera
  GenApi::CIntegerPtr width(camera.GetNodeMap().GetNode("Width"));
  GenApi::CIntegerPtr height(camera.GetNodeMap().GetNode("Height"));

  // create Mat image template
  Mat cv_img(width->GetValue(), height->GetValue(), CV_8UC3);

  // Open video file
  cout << Size(width->GetValue(), height->GetValue()) << endl;
  original = VideoWriter(save_path_ori.c_str(), CV_FOURCC('M', 'P', 'E', 'G'),
                         cFramesPerSecond,
                         Size(width->GetValue(), height->GetValue()), true);

  // Streaming image compression
  vector<int> compression_params;
  compression_params.push_back(CV_IMWRITE_PNG_COMPRESSION);
  compression_params.push_back(3);

  // if the VideoWriter file is not initialized successfully, exit the
  // program.
  if (!original.isOpened()) {
    cout << "ERROR: Failed to write the video (ORIGINAL)" << endl;
  }

  // initiate main loop with algorithm
  while (IsRecording()) {
    try {
      // Wait for an image and then retrieve it. A timeout of 5000
      // ms is used.
      camera.RetrieveResult(5000, ptrGrabResult,
                            TimeoutHandling_ThrowException);
      fc.OutputPixelFormat = PixelType_BGR8packed;

      // Image grabbed successfully?
      if (ptrGrabResult->GrabSucceeded()) {
        // convert to Mat - openCV format for analysis
        fc.Convert(image, ptrGrabResult);
        cv_img = Mat(ptrGrabResult->GetHeight(), ptrGrabResult->GetWidth(), CV_8UC3, (uint8_t*)image.GetBuffer());

        // write the original stream into file
        // cvtColor(cv_img, cv_img, CV_GRAY2BGR);
        original.write(cv_img);

        // write to streaming jpeg
        if (stream_counter == 0) {
          imwrite(
              "/home/agridata/Desktop/embeddedServer/EmbeddedServer/images/"
              "streaming.png",
              cv_img, compression_params);
          stream_counter = 200;
        } else {
          stream_counter--;
        }
		
		if (heartbeat_log == 0) {
			oss << camera.DeviceSerialNumber.GetValue() << "," 
			    << camera.BalanceRatio.GetValue() << "," 
				<< camera.AutoFunctionProfile.GetValue()  << "," 
				<< camera.BalanceRatioSelector.GetValue() << "," 
				<< camera.BalanceWhiteAuto.GetValue() << "," 
				<< camera.BlackLevel.GetValue() << ","
				<< camera.Gamma.GetValue() << ","
				<< camera.ExposureAuto.GetValue() << "," 
				<< camera.ExposureMode.GetValue() << ","
				<< camera.ExposureTime.GetValue() << ","
				<< camera.Gain.GetValue() << "," 
				<< camera.GainAuto.GetValue() << "," 
				<< camera.AcquisitionFrameRate.GetValue() << "," 
				<< camera.AutoGainLowerLimit.GetValue() << "," 
				<< camera.AutoGainUpperLimit.GetValue() << "," 
				<< camera.AutoExposureTimeLowerLimit.GetValue() << "," 
				<< camera.AutoExposureTimeUpperLimit.GetValue() << "," 
				<< camera.AutoTargetBrightness.GetValue()
				<< endl;
			fout << oss.str();
			oss.str("");
			heartbeat_log = 0;
		} else {
			heartbeat_log--;
		}

        // Check file size
		/*
        if (heartbeat == 0) {
          stat(save_path_ori.c_str(), &filestatus);
          int size = filestatus.st_size;
          if (size > 200 * 1024000) {  // 1GB = 1073741824 bytes
            stringstream ss;
            time_t result = time(NULL);
            ss.str(asctime(localtime(&result)));
            string tm = ss.str();
            tm.resize(tm.size() - 1);
            save_path_ori = "/home/agridata/output/" + tm + ".avi";

            original =
                VideoWriter(save_path_ori.c_str(),
                            CV_FOURCC('M', 'P', 'E', 'G'), cFramesPerSecond,
                            Size(width->GetValue(), height->GetValue()), true);
          }
          heartbeat = 200;
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
  SetRecording(false);
  cout << endl << endl << " *** Done ***" << endl << endl;

  return 0;
}

int snap(CBaslerUsbInstantCamera &camera) {
  // Wait for an image and then retrieve it. A timeout of 5000 ms is used.
  camera.RetrieveResult(5000, ptrGrabResult, TimeoutHandling_ThrowException);

  fc.OutputPixelFormat = PixelType_Mono8;

  // Image grabbed successfully?
  if (ptrGrabResult->GrabSucceeded()) {
    // convert to Mat - openCV format for analysis
    fc.Convert(image, ptrGrabResult);
    Mat cv_img = Mat(ptrGrabResult->GetHeight(), ptrGrabResult->GetWidth(),
                     CV_8U, (uint8_t *)image.GetBuffer());
    imwrite("/home/agridata/Desktop/embeddedServer/EmbeddedServer/images/",
            cv_img);
    imshow("original", cv_img);
  } else {
    cout << "Error: " << ptrGrabResult->GetErrorCode() << " ";
    cout << ptrGrabResult->GetErrorDescription() << endl;
  }
  return 0;
}

vector<string> split(const string &s, char delim) {
  stringstream ss(s);
  string item;
  vector<string> tokens;
  while (getline(ss, item, delim)) {
    tokens.push_back(item);
  }
  return tokens;
}

int main() {
  // Set Up Timer
  time_t start, future;
  time(&start);
  double seconds;

  PylonInitialize();

  // Listen on port 4999
  zmq::context_t context(1);
  zmq::socket_t client(context, ZMQ_SUB);
  client.connect("tcp://127.0.0.1:4999");

  // Publish on 4448
  zmq::socket_t publisher(context, ZMQ_PUB);
  publisher.bind("tcp://*:4998");

  client.setsockopt(ZMQ_SUBSCRIBE, "", 0);

  zmq_sleep(1.5);  // Wait for sockets

  int ret;
  CBaslerUsbInstantCamera camera(CTlFactory::GetInstance().CreateFirstDevice());

  // Initialize variables
  string received;
  char **argv;
  int argc = 0;
  size_t pos = 0;
  string s;
  char delimiter = '-';
  string reply;
  ostringstream oss;
  string id_hash;
  vector<string> tokens;
  
  if (IsRecording()) {
	  ret = initialize(ref(camera));
	  thread t(run, ref(camera));
	  t.detach();
  }

  while (true) {
    zmq::message_t messageR;

    if (client.recv(&messageR, ZMQ_NOBLOCK)) {
      received = string(static_cast<char *>(messageR.data()), messageR.size());

      s = received;
	  cout << s << endl;
      tokens = split(s, delimiter);
	  
	  for (int i=0;i<tokens.size();i++) {
		  cout << tokens[i] << endl;
	  }

      try {
        id_hash = tokens[0];

        // Choose action
        if (tokens[1] == "start") {
          if (IsRecording()) {
            oss << id_hash << "_1_AlreadyRecording";
          } else {
            ret = initialize(ref(camera));

            thread t(run, ref(camera));
            t.detach();
            oss << id_hash << "_1_RecordingStarted";
          }
        } else if (tokens[1] == "stop") {
          if (IsRecording()) {
            ret = stop(ref(camera));
            oss << id_hash << "_1_CameraStopped";
          } else {
            oss << id_hash << "_1_AlreadyStopped";
          }
        } else if (tokens[1] == "BalanceWhiteAuto") {
          if (tokens[2] == "BalanceWhiteAuto_Once") {
            camera.BalanceWhiteAuto.SetValue(BalanceWhiteAuto_Once);
            oss << id_hash << "_1_" << tokens[2];
			cout << id_hash << "_1_" << tokens[2];
          } else if (tokens[2] == "BalanceWhiteAuto_Continuous") {
            camera.BalanceWhiteAuto.SetValue(BalanceWhiteAuto_Continuous);
            oss << id_hash << "_1_" << tokens[2];
          } else if (tokens[2] == "BalanceWhiteAuto_Off") {
            camera.BalanceWhiteAuto.SetValue(BalanceWhiteAuto_Off);
            oss << id_hash << "_1_" << tokens[2];
          }
        } else if (tokens[1] == "ExposureAuto") {
          if (tokens[2] == "Once") {
            camera.ExposureAuto.SetValue(ExposureAuto_Once);
            oss << id_hash << "_1_" << tokens[2];
          }
        } else if (tokens[1] == "AutoFunctionProfile") {
          if (tokens[2] == "AutoFunctionProfile_MinimizeExposure") {
            camera.AutoFunctionProfile.SetValue(
                AutoFunctionProfile_MinimizeExposureTime);
            oss << id_hash << "_1_" << tokens[2];
          } else if (tokens[2] == "AutoFunctionProfile_MinimizeGain") {
            camera.AutoFunctionProfile.SetValue(
                AutoFunctionProfile_MinimizeGain);
            oss << id_hash << "_1_" << tokens[2];
          }
        } else if (tokens[1] == "GainAuto") {
          if (tokens[2] == "GainAuto_Once") {
            camera.GainAuto.SetValue(GainAuto_Once);
            oss << id_hash << "_1_" << tokens[2];
          } else if (tokens[2] == "GainAuto_Continuous") {
            camera.GainAuto.SetValue(GainAuto_Continuous);
            oss << id_hash << "_1_" << tokens[2];
          } else if (tokens[2] == "GainAuto_Off") {
            camera.GainAuto.SetValue(GainAuto_Off);
            oss << id_hash << "_1_" << tokens[2];
          }
        } else if (tokens[1] == "ExposureAuto") {
          if (tokens[2] == "Exposure_Once") {
            camera.ExposureAuto.SetValue(ExposureAuto_Once);
            oss << id_hash << "_1_" << tokens[2];
          } else if (tokens[2] == "ExposureAuto_Continuous") {
            camera.ExposureAuto.SetValue(ExposureAuto_Continuous);
            oss << id_hash << "_1_" << tokens[2];
          } else if (tokens[2] == "ExposureAuto_Off") {
            camera.ExposureAuto.SetValue(ExposureAuto_Off);
            oss << id_hash << "_1_" << tokens[2];
          }
        } else if (tokens[1] == "BalanceRatioSelector") {
          if (tokens[2] == "BalanceRatioSelector_Green") {
            camera.BalanceRatioSelector.SetValue(BalanceRatioSelector_Green);
            oss << id_hash << "_1_" << tokens[2];
          } else if (tokens[2] == "BalanceRatioSelector_Red") {
            camera.BalanceRatioSelector.SetValue(BalanceRatioSelector_Red);
            oss << id_hash << "_1_" << tokens[2];
          } else if (tokens[2] == "BalanceRatioSelector_Blue") {
            camera.BalanceRatioSelector.SetValue(BalanceRatioSelector_Blue);
            oss << id_hash << "_1_" << tokens[2];
          }
        } else if (tokens[1] == "GainSelector") {
          camera.GainSelector.SetValue(GainSelector_All);
          oss << id_hash << "_1_" << tokens[1];
        } else if (tokens[1] == "Gain") {
          camera.Gain.SetValue(atof(tokens[1].c_str()));
          oss << id_hash << "_1_" << tokens[1];
        } else if (tokens[1] == "BalanceRatio") {
          camera.BalanceRatio.SetValue(atof(tokens[1].c_str()));
          oss << id_hash << "_1_" << tokens[1];
        } else if (tokens[1] == "AutoTargetBrightness") {
          camera.AutoTargetBrightness.SetValue(atof(tokens[1].c_str()));
          oss << id_hash << "_1_" << tokens[1];
        } else if (tokens[1] == "AutoExposureTimeUpperLimit") {
          camera.AutoExposureTimeUpperLimit.SetValue(atof(tokens[1].c_str()));
          oss << id_hash << "_1_" << tokens[1];
        } else if (tokens[1] == "AutoGainUpperLimit") {
          camera.GainSelector.SetValue(
              GainSelector_All);  // Backup in case we forget
          camera.AutoGainUpperLimit.SetValue(atof(tokens[1].c_str()));
          oss << id_hash << "_1_" << tokens[1];
        } else if (tokens[1] ==
                   "AutoGainLowerLimit") {  // Backup incase we forget
          camera.GainSelector.SetValue(GainSelector_All);
          oss << id_hash << "_1_" << tokens[1];
          camera.AutoGainLowerLimit.SetValue(atof(tokens[1].c_str()));
        } else if (tokens[1] == "GetStatus") {
          oss << id_hash << "_"
              << "Is recording: " << IsRecording() << endl
              << "White Balance Ratio: " << camera.BalanceRatio.GetValue()
              << endl
              << "Auto FunctionProfile: "
              << camera.AutoFunctionProfile.GetValue() << endl
              << "Balance Ratio Selector: "
              << camera.BalanceRatioSelector.GetValue() << endl
              << "White Balance Auto: " << camera.BalanceWhiteAuto.GetValue()
              << endl
              << "Auto Exposure: " << camera.ExposureAuto.GetValue() << endl
              << "Exposure Mode: " << camera.ExposureMode.GetValue() << endl
              << "Gain: " << camera.Gain.GetValue() << endl
			  << "ExposureTime: " << camera.ExposureTime.GetValue() << endl
              << "Gain Auto: " << camera.GainAuto.GetValue() << endl
              << "Framerate: " << camera.AcquisitionFrameRate.GetValue() << endl
			  << "Gain Lower Limit: " << camera.AutoGainLowerLimit.GetValue() << endl
			  << "Gain Upper Limit: " << camera.AutoGainUpperLimit.GetValue() << endl
			  << "Exposure Lower Limit: " << camera.AutoExposureTimeLowerLimit.GetValue() << endl
			  << "Exposure Uppwer Limit: " << camera.AutoExposureTimeUpperLimit.GetValue() << endl
              << "Target Brightness: " << camera.AutoTargetBrightness.GetValue()
              << endl;
        } else {
          oss << id_hash << "_0_CommandNotFound";
        }
      } catch (...) {
        oss << id_hash << "_0_ExceptionProcessingCommand";
      }
	reply = oss.str();
	cout << reply << endl;
    } else {
      //oss << id_hash << "_0_SomethingBad";
    }
    reply = oss.str();
	

    // CLear the string buffer
    oss.str("");

    zmq::message_t messageS(reply.size());
    memcpy(messageS.data(), reply.data(), reply.size());
    publisher.send(messageS);

    // Check time
    seconds = difftime(time(&future),start);
    if (seconds >  60.0 *timelimit) {
      return 0;
    }
  }

  return 0;
}
