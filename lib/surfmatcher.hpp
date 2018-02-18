#include <iostream>
#include <sys/time.h>
#include <time.h>
#include <regex>
#include <sys/types.h>
#include <sys/stat.h>
#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>

#include "opencv2/opencv_modules.hpp"

#ifdef HAVE_OPENCV_XFEATURES2D

#include "opencv2/core.hpp"
#include "opencv2/features2d.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/cudafeatures2d.hpp"
#include "opencv2/xfeatures2d/cuda.hpp" 

using namespace std;
using namespace cv;
using namespace cv::cuda;
using namespace boost::filesystem;

static void help()
{
    cout << "\nThis program demonstrates using SURF_CUDA features detector, descriptor extractor and BruteForceMatcher_CUDA" << endl;
}

void runsift(string previous, string current, string output_dir) 
{
    // Set up images
    GpuMat img1, img2;
    img1.upload(imread(previous, IMREAD_GRAYSCALE));
    CV_Assert(!img1.empty());

    img2.upload(imread(current, IMREAD_GRAYSCALE));
    CV_Assert(!img2.empty());

    // Grab device
    cv::cuda::printShortCudaDeviceInfo(cv::cuda::getDevice());

    // Construction
    SURF_CUDA surf(200, 4, 2, false, 0.01f, true);
    // SURF_CUDA surf;

    // detecting keypoints & computing descriptors
    GpuMat keypoints1GPU, keypoints2GPU;
    GpuMat descriptors1GPU, descriptors2GPU;

    // Timing [begin]
    struct timeval tp;
    time_t rawtime; time(&rawtime);
    long int ms0, ms1;
    gettimeofday(&tp, NULL);
    ms0 = tp.tv_sec * 1000 + tp.tv_usec / 1000;

    // Go SURFing
    surf(img1, GpuMat(), keypoints1GPU, descriptors1GPU);
    surf(img2, GpuMat(), keypoints2GPU, descriptors2GPU);

    // Timing [end]
    gettimeofday(&tp, NULL);
    ms1 = tp.tv_sec * 1000 + tp.tv_usec / 1000;
    printf("\nTime: %ld\n", ms1-ms0);

    // Number of points
    cout << "FOUND " << keypoints1GPU.cols << " keypoints on first image (" << previous << ")" << endl;
    cout << "FOUND " << keypoints2GPU.cols << " keypoints on second image (" << current << ")" << endl;

    // Matching descriptors
    Ptr<cv::cuda::DescriptorMatcher> matcher = cv::cuda::DescriptorMatcher::createBFMatcher(surf.defaultNorm());
    vector<DMatch> matches;
    matcher->match(descriptors1GPU, descriptors2GPU, matches);

    // Get frame numbers
    const regex r("(\\d+).png");  
    smatch sm;

    // Current frame number
    regex_search(current, sm, r);
    string cidx = sm[1];

    // Previous frame number
    regex_search(previous, sm, r);
    string pidx = sm[1];

    // Save
    string outname = output_dir + cidx + "-" + pidx;

    // Downloading results
    vector<KeyPoint> keypoints1, keypoints2;
    vector<float> descriptors1, descriptors2;
    surf.downloadKeypoints(keypoints1GPU, keypoints1);
    surf.downloadKeypoints(keypoints2GPU, keypoints2);
    surf.downloadDescriptors(descriptors1GPU, descriptors1);
    surf.downloadDescriptors(descriptors2GPU, descriptors2);

    // Save to text
    FileStorage m(outname + "_matches.xml", FileStorage::WRITE);
    for (DMatch & element : matches) {
        write( m, "distance", element.distance );
        write( m, "queryIdx", element.queryIdx );
        write( m, "trainIdx", element.trainIdx );
    }
    m.release();

    FileStorage kp1(outname + "_keypoints1.xml", FileStorage::WRITE);
    for (KeyPoint & element : keypoints1) {
        write( kp1, "pt", element.pt );
        write( kp1, "size", element.size );
        write( kp1, "angle", element.angle );
    }
    kp1.release(); 

    FileStorage kp2(outname + "_keypoints2.xml", FileStorage::WRITE);
    for (KeyPoint & element : keypoints2) {
        write( kp2, "pt", element.pt );
        write( kp2, "size", element.size );
        write( kp2, "angle", element.angle );
    }
    kp2.release();

    // Save image
    Mat img_matches;
    drawMatches(Mat(img1), keypoints1, Mat(img2), keypoints2, matches, img_matches);
    imwrite(outname + ".png", img_matches);
}

int main(int argc, char *argv[]) {
    string data_dir;

    if (argc < 2)
    {
        cout << "Please use png folder as first and only argument";
    } 
    else 
    {
        data_dir = argv[1];
    }

    string output_dir = data_dir + "output/";
    
    int status;
    status = mkdir(output_dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if (status == 0) {
      cout << "Made output directory: " << output_dir << endl;
    } else {
      cout << "Makedir failed but continuing anyway" << endl;
    }

    // Convert to Boost Path
    path p(argc>1? argv[1] : data_dir);

    try 
    {
      if (exists(p)) 
      {
        if (is_directory(p)) 
        {
          // cout << "\n\nSifting images from: " << p << "\n";

          // Vector to store paths
          typedef vector<path> vec;                
          vec v;

          // Copy paths to vector and sort
          
          copy(directory_iterator(p), directory_iterator(), back_inserter(v));
          sort(v.begin(), v.end());
          // Generate pairs of consequtive paths (image files)
          vector< pair <path, path> > pairs;
          for (vec::const_iterator it(v.begin()), it_end(v.end()); it != it_end; ++it)
          {
            try {
                  pairs.push_back(make_pair(*it--, *it++));
            } catch( bad_alloc ) {
                  cout << "Bad allocation" << std::endl;

                  // No need to call delete[] since the allocation failed. Loop again and try
                  // to allocate the memory again.
                  continue;

            }
          }
          
          // The last pair isn't really a pair
          pairs.pop_back(); 

          // cout << pairs.size() << " pairs found\n";
          // cout << "Sifting. . .\n\n";

          string current, previous;
          for ( vector < pair<path,path> >::const_iterator it = pairs.begin() ; it != pairs.end () ; it++)
          {
              current = boost::filesystem::canonical(it->first).string();; 
              previous = boost::filesystem::canonical(it->second).string();

              // cout << "Current: " << current << "\nPrevious: " << previous << "\n\n";
              try {
                runsift(previous, current, output_dir);
              } catch (...) {
                cout << "Passing\n";
              }
          }

        }

        else
          cout << p << " exists, but is neither a regular file nor a directory\n";
      }

      else
        cout << p << " does not exist\n";
    }


    catch (const filesystem_error& ex)
    {
      cout << ex.what() << '\n';
    }
}

#else

int main()
{
    cerr << "OpenCV was built without xfeatures2d module" << endl;
    return 0;
}

#endif