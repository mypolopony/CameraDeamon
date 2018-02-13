/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by The HDF Group.                                               *
 * Copyright by the Board of Trustees of the University of Illinois.         *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of HDF5.  The full HDF5 copyright notice, including     *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the root of the source code       *
 * distribution tree, or in https://support.hdfgroup.org/ftp/HDF5/releases.  *
 * If you do not have access to either file, you may request a copy from     *
 * help@hdfgroup.org.                                                        *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <iostream>
#include <dirent.h>
#include <typeinfo>
#include <opencv2/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <boost/algorithm/string/predicate.hpp>

#ifdef __cplusplus
extern "C"
{
#endif

#include "hdf5.h"
#include "hdf5_hl.h"

#ifdef __cplusplus
}
#endif

#define WIDTH         600
#define HEIGHT        960

struct img {
  unsigned char *data;
};

using namespace cv;
using namespace std;


vector<img> getImages(string path) {
	DIR*    dir;
	vector<img> images(144);
	dirent* pdir;

    dir = opendir(path.c_str());

    while (pdir = readdir(dir)) {
		string filename = pdir->d_name;

		if (boost::algorithm::ends_with(filename, "jpg")) {
			img curr_img;
			Mat mat_img;

			string fullpath = path + "/" + filename;
			cout << fullpath << endl;

			mat_img = imread(fullpath, CV_LOAD_IMAGE_COLOR);
			cvtColor(mat_img, mat_img, COLOR_BGR2RGB);
			cout << sizeof(&mat_img.data)*WIDTH*HEIGHT << endl;
			memcpy(&curr_img.data, &mat_img.data, sizeof(&mat_img.data)*WIDTH*HEIGHT);
        	images.push_back(curr_img);
		}
    }
    
    return images;
}

static void writeAll(vector<img> mats) {

	hid_t file_id = H5Fcreate( "ex_image1.h5", H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT );

	for (int i=0; i<mats.size(); i++) {
		
		/* make the image */
		H5IMmake_image_24bit( file_id, to_string(i).c_str(), WIDTH, HEIGHT, "INTERLACE_PIXEL", (unsigned char*) mats[i].data );

	}

	/* close the file. */
	H5Fclose( file_id );
}


int main( void )
{
	vector<img> images = getImages("/data/output/2018-02-08_19-26/0030531BF616/19/26");
	writeAll(images);

	return 0;
}
