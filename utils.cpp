#include <stdio.h>
#include <iostream>
#include <chrono>

// AgriData
#include "AGDUtils.h"

// Include files to use openCV
#include "opencv2/core.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"

// Redis
#include <redox.hpp>

// Logging
#include <easylogging++.h>

INITIALIZE_EASYLOGGINGPP

void SampleRedis() {
    redox::Redox rdx;
    std::string s;
    
    // Connect
    if (rdx.connect() == 1) {
        
        // Single pop from detection
        /*
        rdx.command<std::string>({"LPOP", "detection"}, [](redox::Command<std::string>& c) {
          if(c.ok()) {
            std::cout << "Hello, async " << c.reply() << std::endl;
          } else {
            std::cerr << "Command has error code " << c.status() << std::endl;
          }
        });
         * */
        
        // More appropriate looping command repeats until cmd.free()
        redox::Command<std::string>& cmd = rdx.commandLoop<std::string>({"LPOP", "detection"}, [](redox::Command<std::string>& c) {
          if(c.ok()) 
              std::cout << c.cmd() << ": " << c.reply() << std::endl;
        }, 1);

        std::this_thread::sleep_for(std::chrono::seconds(10));
        
        cmd.free();
        rdx.disconnect();
    }
}


void SampleRead()
{
    std::string filename = "/data/output/2018-02-13_16-49/0030531BF616/16_50.hdf5";

    // Initialize
    ImageReader reader;

    // Read in the HDF5 File
    reader.read(filename);

    // Elements is a public vector<string> with the names of the imags in the HDF5 file
    std::cout << reader.elements[0] << std::endl;

    // Use next() to grab the next image -- EOF will be raised if the end is reached (can improve this)
    cv::Mat n = reader.next();

    // Verify the image has been grabbed
    imwrite("test_reader.jpg", n);
}
