/*
 * File:   AGDUtils.h
 * Author: agridata
 *
 * Created on March 13, 2017, 3:41 PM
 */

#ifndef AGDUTILS_H
#define AGDUTILS_H

#include <vector>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

// Include files to use openCV
#include "opencv2/core.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"


#ifdef __cplusplus
extern "C" {
#endif

#include "hdf5.h"
#include "hdf5_hl.h"

#ifdef __cplusplus
}
#endif

#define MAX_NAME 1024

namespace AGDUtils {
    bool mkdirp(const char* path, mode_t mode);
    std::vector <std::string> split(const std::string &s, char delim);
    std::string readableTimestamp();
    std::string grabTime(std::string format);
    int64_t grabSeconds();
    int64_t grabMilliseconds();
    std::string pipe_to_string(const char *command);
}

class ImageReader {
public:
    // Constructor / Destructor
    ImageReader();
    virtual ~ImageReader();

    // Methods
    void read(std::string filename);
    cv::Mat next();

    // Attributes
    std::vector<std::string> elements;
    hsize_t numObjects;


private:
    hid_t fid, grp;
    hsize_t width, height;
    size_t totalsize;
    char group_name[MAX_NAME];
    unsigned int idx;
};

#endif /* AGDUTILS_H */
