#include <iostream>
#include <math.h>
#include <cmath>
#include <regex>
#include <iomanip>
#include <regex>
#include <ctime>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>

#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "cudaImage.h"
#include "cudaSift.h"

#define MAX_DATE 12

using namespace boost::filesystem;

int ImproveHomography(SiftData &data, float *homography, int numLoops, float minScore, float maxAmbiguity, float thresh);
void PrintMatchData(SiftData &siftData1, SiftData &siftData2, CudaImage &img, std::string current, std::string previous);
void MatchAll(SiftData &siftData1, SiftData &siftData2, float *homography);

double ScaleUp(CudaImage &res, CudaImage &src);

// Safe refuge for lost globals
int threshold_value = 0;
int threshold_type = 3;;
int const max_value = 255;
int const max_type = 4;
int const max_BINARY_value = 255;

// Current date function
std::string get_date(void)
{
   time_t now;
   char the_date[MAX_DATE];

   the_date[0] = '\0';

   now = time(NULL);

   if (now != -1)
   {
      strftime(the_date, MAX_DATE, "%H%M%S_%d%m%Y", gmtime(&now));
   }

   return std::string(the_date);
}


// Output directory
std::string d = get_date();
std::string outpath = "data/" + d + "/";

void runsift(std::string previous, std::string current)
{
  int devNum = 0;

  // Read images in grayscale
  cv::Mat _limg, _rimg, limg, rimg;
  cv::imread(previous, 0).convertTo(_limg, CV_32FC1);
  cv::imread(current, 0).convertTo(_rimg, CV_32FC1);

  // Invert for edge detection
  threshold(_limg, limg, threshold_value, max_BINARY_value, threshold_type);
  threshold(_rimg, rimg, threshold_value, max_BINARY_value, threshold_type);
  unsigned int w = limg.cols;
  unsigned int h = limg.rows;
  std::cout << "Image size = (" << w << "," << h << ")" << std::endl;

  // Initialize CUDA and download images to device
  std::cout << "Initializing data..." << std::endl;
  InitCuda(devNum);
  CudaImage img1, img2;
  img1.Allocate(w, h, iAlignUp(w, 128), false, NULL, (float*)limg.data);
  img2.Allocate(w, h, iAlignUp(w, 128), false, NULL, (float*)rimg.data);
  img1.Download();
  img2.Download();

  // Extract Sift features from images
  SiftData siftData1, siftData2;
  float initBlur = 1.0f;
  float thresh = 3.5f;
  InitSiftData(siftData1, 32768, true, true);
  InitSiftData(siftData2, 32768, true, true);

  ExtractSift(siftData1, img1, 1, initBlur, thresh, 0.0f, false);
  ExtractSift(siftData2, img2, 1, initBlur, thresh, 0.0f, false);

  MatchSiftData(siftData1, siftData2);
  float homography[9];
  int numMatches;
  FindHomography(siftData1, homography, &numMatches, 10000, 0.00f, 0.80f, 5.0);
  int numFit = ImproveHomography(siftData1, homography, 5, 0.00f, 0.80f, 3.0);

  std::cout << "\nNumber of original features: " <<  siftData1.numPts << " " << siftData2.numPts << std::endl;
  std::cout << "Number of matching features: " << numFit << " " << numMatches << " " << 100.0f*numFit/std::min(siftData1.numPts, siftData2.numPts) << "% " << initBlur << " " << thresh << std::endl;

  // Print out and store summary data
  PrintMatchData(siftData1, siftData2, img1, previous, current);
  const std::regex r("(\\d+).jpg");
  std::smatch sm;

  regex_search(current, sm, r);
  std::string cidx = sm[1];

  regex_search(previous, sm, r);
  std::string pidx = sm[1];

  std::string d = get_date();
  std::string outname = outpath + cidx + "_" + pidx + ".pgm";
  cv::imwrite(outname, limg);

  // Free Sift data from device
  FreeSiftData(siftData1);
  FreeSiftData(siftData2);
}

void MatchAll(SiftData &siftData1, SiftData &siftData2, float *homography)
{
#ifdef MANAGEDMEM
  SiftPoint *sift1 = siftData1.m_data;
  SiftPoint *sift2 = siftData2.m_data;
#else
  SiftPoint *sift1 = siftData1.h_data;
  SiftPoint *sift2 = siftData2.h_data;
#endif
  int numPts1 = siftData1.numPts;
  int numPts2 = siftData2.numPts;
  int numFound = 0;
  for (int i=0;i<numPts1;i++) {
    float *data1 = sift1[i].data;
    std::cout << i << ":" << sift1[i].scale << ":" << (int)sift1[i].orientation << std::endl;
    bool found = false;
    for (int j=0;j<numPts2;j++) {
      float *data2 = sift2[j].data;
      float sum = 0.0f;
      for (int k=0;k<128;k++)
  sum += data1[k]*data2[k];
      float den = homography[6]*sift1[i].xpos + homography[7]*sift1[i].ypos + homography[8];
      float dx = (homography[0]*sift1[i].xpos + homography[1]*sift1[i].ypos + homography[2]) / den - sift2[j].xpos;
      float dy = (homography[3]*sift1[i].xpos + homography[4]*sift1[i].ypos + homography[5]) / den - sift2[j].ypos;
      float err = dx*dx + dy*dy;
      if (err<100.0f)
  found = true;
      if (err<100.0f || j==sift1[i].match) {
  if (j==sift1[i].match && err<100.0f)
    std::cout << " *";
  else if (j==sift1[i].match)
    std::cout << " -";
  else if (err<100.0f)
    std::cout << " +";
  else
    std::cout << "  ";
  std::cout << j << ":" << sum << ":" << (int)sqrt(err) << ":" << sift2[j].scale << ":" << (int)sift2[j].orientation << std::endl;
      }
    }
    std::cout << std::endl;
    if (found)
      numFound++;
  }
  std::cout << "Number of points found: " << numFound << std::endl;
}

void PrintMatchData(SiftData &siftData1, SiftData &siftData2, CudaImage &img, std::string previous, std::string current)
{
  int numPts = siftData1.numPts;
#ifdef MANAGEDMEM
  SiftPoint *sift1 = siftData1.m_data;
  SiftPoint *sift2 = siftData2.m_data;
#else
  SiftPoint *sift1 = siftData1.h_data;
  SiftPoint *sift2 = siftData2.h_data;
#endif
  float *h_img = img.h_data;
  int w = img.width;
  int h = img.height;
  std::cout << std::setprecision(3);
  for (int j=0;j<numPts;j++) {
    int k = sift1[j].match;
    if (sift1[j].match_error<5) {
      float dx = sift2[k].xpos - sift1[j].xpos;
      float dy = sift2[k].ypos - sift1[j].ypos;

      // Format: previous,current,px,py,cx,cy,score,ambiguity,scale,error,porientation,corientation,dx,dy
      std::cout << previous << "," << current << ",";
      std::cout << (int)sift1[j].xpos << "," << (int)sift1[j].ypos << ",";
      std::cout << (int)sift2[k].xpos << "," << (int)sift2[k].ypos << ",";
      std::cout << sift1[j].score << "," << sift1[j].ambiguity << ",";
      std::cout << sift1[j].scale << ",";
      std::cout << (int)sift1[j].match_error << ",";
      std::cout << (int)sift1[j].orientation << "," << (int)sift2[k].orientation << ",";
      std::cout << dx << "," << dy << std::endl;
    }
  }
}



 /*
// CUDA kernel to add elements of two arrays
__global__
void add(int n, float *x, float *y)
{
  int index = blockIdx.x * blockDim.x + threadIdx.x;
  int stride = blockDim.x * gridDim.x;
  for (int i = index; i < n; i += stride)
    y[i] = x[i] + y[i];
}

int simplesift(void)
{
  int N = 1<<20;
  float *x, *y;

  // Allocate Unified Memory -- accessible from CPU or GPU
  cudaMallocManaged(&x, N*sizeof(float));
  cudaMallocManaged(&y, N*sizeof(float));

  // initialize x and y arrays on the host
  for (int i = 0; i < N; i++) {
    x[i] = 1.0f;
    y[i] = 2.0f;
  }

  // Launch kernel on 1M elements on the GPU
  int blockSize = 256;
  int numBlocks = (N + blockSize - 1) / blockSize;
  add<<<numBlocks, blockSize>>>(N, x, y);

  // Wait for GPU to finish before accessing on host
  cudaDeviceSynchronize();

  // Check for errors (all values should be 3.0f)
  float maxError = 0.0f;
  for (int i = 0; i < N; i++)
    maxError = fmax(maxError, fabs(y[i]-3.0f));
  std::cout << "Max error: " << maxError << std::endl;

  // Free memory
  cudaFree(x);
  cudaFree(y);

  return 0;
}
*/
int main (int argc, char *argv[]) {
    std::string data_dir;

    if (argc < 2)
    {
        data_dir = "/data/CudaSift/data/2017-08-17_10-25/raw";
    }
    else
    {
        data_dir = argv[1];
    }

    // Convert to Boost Path
    path p(argc>1? argv[1] : data_dir);

    try
    {
      if (exists(p))
      {
        if (is_directory(p))
        {
          std::cout << "\n\nSifting images from: " << p << "\n";

          // Vector to store paths
          typedef std::vector<path> vec;
          vec v;

          // Copy paths to vector and sort
          copy(directory_iterator(p), directory_iterator(), back_inserter(v));
          sort(v.begin(), v.end());

          // Generate pairs of consequtive paths (image files)
          std::vector< std::pair <path, path> > pairs;
          for (vec::const_iterator it(v.begin()), it_end(v.end()); it != it_end; ++it)
          {
            pairs.push_back(std::make_pair(*it--, *it++));
          }

          // The last pair isn't really a pair
          pairs.pop_back();

          std::cout << pairs.size() << " pairs found\n";
          std::cout << "Sifting. . .\n\n";

          std::string current, previous;
          for ( std::vector < std::pair<path,path> >::const_iterator it = pairs.begin() ; it != pairs.end () ; it++)
          {
              current = boost::filesystem::canonical(it->first).string();;
              previous = boost::filesystem::canonical(it->second).string();

              std::cout << "Current: " << current << "\nPrevious: " << previous << "\n\n";
              runsift(previous, current);
          }

        }

        else
          std::cout << p << " exists, but is neither a regular file nor a directory\n";
      }

      else
        std::cout << p << " does not exist\n";
    }


    catch (const filesystem_error& ex)
    {
      std::cout << ex.what() << '\n';
    }
}