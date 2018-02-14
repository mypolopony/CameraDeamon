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
#define MAX_NAME 1024

struct img {
  uint8_t data[HEIGHT*WIDTH];
};

using namespace cv;
using namespace std;


vector<img> getImages(string path) {
	DIR*    dir;
	vector<img> images(144);
	dirent* pdir;

    dir = opendir(path.c_str());
	hid_t file_id = H5Fcreate( "ex_image1.h5", H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT );

    while (pdir = readdir(dir)) {
		string filename = pdir->d_name;

		if (boost::algorithm::ends_with(filename, "24.jpg")) {
			img curr_img;
			Mat mat_img;

			string fullpath = path + "/" + filename;
			cout << fullpath << endl;

			mat_img = imread(fullpath, CV_LOAD_IMAGE_COLOR);
			cvtColor(mat_img, mat_img, COLOR_BGR2RGB);
			
			int size = HEIGHT*WIDTH*sizeof(uint8_t);
			cout << size << endl;

			memcpy(curr_img.data, mat_img.data, size);
        	images.push_back(curr_img);

			
			H5IMmake_image_24bit( file_id, filename.c_str(), HEIGHT, WIDTH, "INTERLACE_PIXEL", mat_img.data );
		}
    }
	H5Fclose( file_id );
    
    return images;
}

static void writeAll(vector<img> mats) {

	hid_t file_id = H5Fcreate( "ex_image1.h5", H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT );

	for (int i=0; i<mats.size(); i++) {
		
		/* make the image */
		H5IMmake_image_24bit( file_id, to_string(i).c_str(), HEIGHT, WIDTH, "INTERLACE_PIXEL", (uint8_t *) mats[i].data );

	}

	/* close the file. */
	H5Fclose( file_id );
}

static void readHDF5(string filename) {
	hid_t fid, grp, imgid;
	hid_t lapl_id;
	herr_t err;
	ssize_t len;
	hsize_t width = 513;
	hsize_t height = 641;
	hsize_t nobj;
	Mat img;
	size_t totalsize = 3 * (size_t)width * (size_t)height * (size_t)sizeof(uint8_t);
	char group_name[MAX_NAME];
	char memb_name[MAX_NAME];
	uint8_t buf[totalsize];
	string out;

	cout << "Buffer Size: " << totalsize << endl;

	fid = H5Fopen(filename.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);
	cout << "Read " << filename << endl;

	grp = H5Gopen(fid, "/", H5P_DEFAULT);

	H5Iget_name(grp, group_name, MAX_NAME); 
	cout << "Group Name: " << group_name << endl;

	err = H5Gget_num_objs(grp, &nobj);
	cout << "Number of Objects: " << nobj << endl;

	for (hsize_t i = 0; i < 1; i++) {
		cout << " Member #" << i << ": " << endl;
		H5Gget_objname_by_idx(grp, i, memb_name, MAX_NAME);
		cout << "  Name: " << memb_name << endl;

		imgid = H5Oopen ( grp, memb_name, H5P_DEFAULT );
		err = H5IMis_image(grp, memb_name);
		cout << "Is image? " << err << endl;
		err = H5IMread_image(grp, memb_name, buf);
		cout << "Read? " << err << endl;

		img = Mat(height, width, CV_8UC3, (uchar *) buf);	
		cvtColor(img, img, CV_BGR2RGB);

		// Write the last image
		out = memb_name;
		out += ".jpg";
		imwrite(out, img);
	}
	
	return;
}


int main( void )
{
	// vector<img> images = getImages("/data/output/2018-02-08_19-26/0030531BF616/19/26");
	//writeAll(images);

	string filename = "/data/output/2018-02-13_16-49/0030531BF616/16_50.hdf5";
	readHDF5(filename);

	return 0;
}
