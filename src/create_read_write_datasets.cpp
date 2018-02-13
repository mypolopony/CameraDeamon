/**
 * @file create_read_write_datasets.cpp
 * @author Fangjun Kuang <csukuangfj dot at gmail dot com>
 * @date December 2017
 *
 * @brief It demonstrates how to create a dataset,  how
 * to write a cv::Mat to the dataset and how to
 * read a cv::Mat from it.
 *
 */

//! [tutorial]
#include <iostream>

#include <opencv2/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/hdf.hpp>

#include "hdf5.h"
#include "hdf5_hl.h"


#define WIDTH         400
#define HEIGHT        200
#define PAL_ENTRIES   9

using namespace cv;
using namespace std;

unsigned char buf [ WIDTH*HEIGHT ];

static void write_root_group_single_channel()
{
    String filename = "rtgrpsingle_channel.h5";
    String dataset_name = "/single"; // Note that it is a child of the root group /

    // prepare data
    Mat data;
    data = (cv::Mat_<float>(2, 3) << 0, 1, 2, 3, 4, 5, 6);

    //! [tutorial_open_file]
    Ptr<hdf::HDF5> h5io = hdf::open(filename);
    //! [tutorial_open_file]

    //! [tutorial_write_root_single_channel]
    // write data to the given dataset
    // the dataset "/single" is created automatically, since it is a child of the root
    h5io->dswrite(data, dataset_name);
    //! [tutorial_write_root_single_channel]

    //! [tutorial_read_dataset]
    Mat expected;
    h5io->dsread(expected, dataset_name);
    //! [tutorial_read_dataset]

    //! [tutorial_check_result]
    double diff = norm(data - expected);
    CV_Assert(abs(diff) < 1e-10);
    //! [tutorial_check_result]

    h5io->close();
}

static void write_single_channel()
{
    String filename = "single_channel.h5";
    String parent_name = "/data";
    String dataset_name = parent_name + "/single";

    // prepare data
    Mat data;
    data = (cv::Mat_<float>(2, 3) << 0, 1, 2, 3, 4, 5);

    Ptr<hdf::HDF5> h5io = hdf::open(filename);

    //! [tutorial_create_dataset]
    // first we need to create the parent group
    if (!h5io->hlexists(parent_name)) h5io->grcreate(parent_name);

    // create the dataset if it not exists
    if (!h5io->hlexists(dataset_name)) h5io->dscreate(data.rows, data.cols, data.type(), dataset_name);
    //! [tutorial_create_dataset]

    // the following is the same with the above function write_root_group_single_channel()

    h5io->dswrite(data, dataset_name);

    Mat expected;
    h5io->dsread(expected, dataset_name);

    double diff = norm(data - expected);
    CV_Assert(abs(diff) < 1e-10);

    h5io->close();
}

/*
 * creating, reading and writing multiple-channel matrices
 * are the same with single channel matrices
 */
static void write_multiple_channels()
{
    String filename = "two_channels.h5";
    String parent_name = "/data";
    String dataset_name = parent_name + "/two_channels";

    // prepare data
    Mat data(2, 3, CV_32SC2);
    for (size_t i = 0; i < data.total()*data.channels(); i++)
        ((int*) data.data)[i] = (int)i;

    Ptr<hdf::HDF5> h5io = hdf::open(filename);

    // first we need to create the parent group
    if (!h5io->hlexists(parent_name)) h5io->grcreate(parent_name);

    // create the dataset if it not exists
    if (!h5io->hlexists(dataset_name)) h5io->dscreate(data.rows, data.cols, data.type(), dataset_name);

    // the following is the same with the above function write_root_group_single_channel()

    h5io->dswrite(data, dataset_name);

    Mat expected;
    h5io->dsread(expected, dataset_name);

    double diff = norm(data - expected);
    CV_Assert(abs(diff) < 1e-10);

    h5io->close();
}

static void _demo() {
	// dual channel hilbert matrix
	Mat A, B;

	A = imread("/data/output/2018-02-08_19-26/0030531BF616/19/26/143.jpg", CV_LOAD_IMAGE_COLOR); 
	B = imread("/data/output/2018-02-08_19-26/0030531BF616/19/26/1.jpg", CV_LOAD_IMAGE_COLOR); 
	
	int rows = A.rows;
	int cols = A.cols;
	int numimages = 2;


	// open / autocreate hdf5 file
	cv::Ptr<cv::hdf::HDF5> h5io = cv::hdf::open( "1143.h5" );

	// optimise dataset by two chunks
	int chunks[2] = { rows*cols, rows*cols*2 };

	// create 100x100 CV_64FC2 compressed space
	h5io->dscreate( rows, cols, CV_8UC3, "A", 9);
	h5io->dswrite( A, "A" );

	h5io->dscreate( rows, cols, CV_8UC3, "B", 9);
	h5io->dswrite( B, "B" );

	h5io->close();
}

void demo() {
	hid_t         file_id;
	hsize_t       pal_dims[] = {PAL_ENTRIES,3};
	size_t        i, j;
	int           n, space;
	unsigned char pal[PAL_ENTRIES*3] = {  /* create a palette with 9 colors */
	0,0,168,      /* dark blue */
	0,0,252,      /* blue */
	0,168,252,    /* ocean blue */
	84,252,252,   /* light blue */
	168,252,168,  /* light green */
	0,252,168,    /* green */
	252,252,84,   /* yellow */
	252,168,0,    /* orange */
	252,0,0};     /* red */

	/* create an image of 9 values divided evenly by the array */
	space = WIDTH*HEIGHT / PAL_ENTRIES;
	for (i=0, j=0, n=0; i < WIDTH*HEIGHT; i++, j++ )
	{
		buf[i] = n;
		
		if ( j > space )
		{
			n++;
			j=0;
		}
		
		if (n>PAL_ENTRIES-1) n=0;
	}

	/* create a new HDF5 file using default properties. */
	file_id = H5Fcreate( "ex_image1.h5", H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT );

	/* make the image */
	H5IMmake_image_8bit( file_id, "image1", (hsize_t)WIDTH, (hsize_t)HEIGHT, buf );

	/* make a palette */
	H5IMmake_palette( file_id, "pallete", pal_dims, pal );

	/* attach the palette to the image */
	H5IMlink_palette( file_id, "image1", "pallete" );

	/* close the file. */
	H5Fclose( file_id );
}

int main()
{
    //write_root_group_single_channel();

    //write_single_channel();

    //write_multiple_channels();

	demo();

    return 0;
}
//! [tutorial]
