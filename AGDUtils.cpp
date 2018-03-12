/*
 * File:   AGDUtils.cpp
 * Author: agridata
 *
 * Created on March 13, 2017, 1:33 PM
 */

#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "AGDUtils.h"
#include <sys/stat.h>
#include <errno.h>
#include <iostream>
#include <chrono>


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

class popen;
using namespace std;
using namespace cv;

#define DEFAULT_MODE      S_IRWXU | S_IRGRP |  S_IXGRP | S_IROTH | S_IXOTH

/**
 * Constructor
 */
ImageReader::ImageReader() {
    idx = 0;
    width = 513;
    height = 641;
    totalsize = 3 * (size_t) width * (size_t) height * (size_t)sizeof (uint8_t);

}

/**
 * Destructor
 */
ImageReader::~ImageReader() {
}

void ImageReader::read(string filename) {
    // Open file
    fid = H5Fopen(filename.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);

    // Open group
    grp = H5Gopen(fid, "/", H5P_DEFAULT);

    // Group name
    H5Iget_name(grp, group_name, MAX_NAME);

    // Enumerate objects
    H5Gget_num_objs(grp, &numObjects);

    // Gather list of names
    for (unsigned int i = 0; i < numObjects; i++) {
        char memb_name[MAX_NAME];
        H5Gget_objname_by_idx(grp, i, memb_name, MAX_NAME);
        elements.push_back((string) memb_name);
    }

    return;
}

/**
 * ImageReader::next
 *
 * This methods serves to access the opened file as if it were an iterator;
 * The implementation is very simple and relies only on idx
 */
Mat ImageReader::next() {
    if (idx < numObjects) {
        uint8_t buf[totalsize];

        H5IMread_image(grp, elements[idx].c_str(), buf);

        // Create the CV Image
        Mat img = Mat(height, width, CV_8UC3, (uchar *) buf);
        cvtColor(img, img, CV_BGR2RGB);

        // Increment
        idx++;

        return img;
    } else {
        throw ("EOF");
    }
}

namespace AGDUtils {

    /**
     * mkdirp
     *
     * Implemention of mkdir -p
     */

    bool mkdirp(const char* path, mode_t mode = DEFAULT_MODE) {
        // Invalid string
        if (path[0] == '\0') {
            return false;
        }

        char* p = const_cast<char*> (path); // const cast for hack

        while (*p != '\0') { // Find next slash mkdir() it and until we're at end of string
            p++; // Skip first character

            while (*p != '\0' && *p != '/') p++; // Find first slash or end

            char v = *p; // Remember value from p
            *p = '\0'; // Write end of string at p
            if (mkdir(path, mode) != 0 && errno != EEXIST) { // Create folder from path to '\0' inserted at p
                *p = v;
                return false;
            }
            *p = v; // Restore path to it's former glory
        }
        return true;
    }

    /**
     * split
     *
     * Splits a string based on delim
     */
    vector <string> split(const string &s, char delim) {
        stringstream ss(s);
        string item;
        vector <string> tokens;
        while (getline(ss, item, delim)) {
            tokens.push_back(item);
        }
        return tokens;
    }

    /**
     * grabTime
     *
     * A call to strftime returns a timestamp
     */
    string grabTime(string format = "%Y-%m-%d_%H-%M-%S") {
        time_t rawtime;
        struct tm * timeinfo;
        char buffer [80];

        time(&rawtime);
        timeinfo = localtime(&rawtime);

        strftime(buffer, 80, format.c_str(), timeinfo);
        return string(buffer);
    }

    /** grabSeconds
     *
     * grabTime is a way to get a string from the shell, but this is the more canonical
     * way to grab seconds since 1970, returning an int64_t

     */
    int64_t grabSeconds() {
        time_t now;
        time(&now);
        int64_t itime = *((int64_t*) & now);

        return (itime);
    }

    /** Chrono-enabled millisecond timestamp
     * 
     * Compatible with incoming IMU data
     */
    int64_t grabMilliseconds() {
        std::chrono::milliseconds ms = std::chrono::duration_cast< std::chrono::milliseconds >(
                std::chrono::system_clock::now().time_since_epoch()
                );
        return (int64_t) ms.count();
    }

    /**
     * pipe_to_string
     *
     * Grabs the results of a bash command as a string
     */
    string pipe_to_string(const char *command) {
        char buffer[128];
        FILE* pipe = popen(command, "r");
        if (pipe) {
            while (!feof(pipe)) {
                if (fgets(buffer, 128, pipe) != NULL) {
                }
            }
            pclose(pipe);
            buffer[strlen(buffer) - 1] = '\0';
        }
        return (string) buffer;
    }
}
