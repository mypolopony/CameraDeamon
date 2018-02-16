#include <stdio.h>
#include <iostream>
#include "AGDUtils.h"

// Include files to use openCV
#include "opencv2/core.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"

int SampleRead()
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

    return 0;
}
