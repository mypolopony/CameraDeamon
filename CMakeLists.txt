cmake_minimum_required(VERSION 3.7)
project(CameraDeamon)

set(CMAKE_CXX_STANDARD 11)
add_compile_options(-std=c++11)

set(SOURCE_FILES
    lib/json.hpp
    lib/zhelpers.hpp
    lib/zmq.hpp
    AGDUtils.cpp
    AGDUtils.h
    AgriDataCamera.cpp
    AgriDataCamera.h
    main.cpp)

find_package( OpenCV REQUIRED )
find_package( Pylon REQUIRED )

FIND_PATH(	PYLON_INCLUDE_DIR pylon/PylonBase.h
        PATHS
        /opt/pylon5/include
        "$ENV{PYLON_ROOT}/include"
        )

FIND_LIBRARY(	PYLONBASE_LIBRARY
        NAMES
        pylonbase PylonBase_MD_VC100
        PATHS
        ${PYLON_LIBRARY}
        )

INCLUDE_DIRECTORIES(/opt/pylon5/include/ /usr/include/opencv2/ /home/agridata/CameraDeamon/lib/ /home/agridata/CameraDeamon/CameraDeamon/library/)

LINK_DIRECTORIES(/opt/pylon5/lib64 /home/agridata/CameraDeamon/ /usr/include/lib)
LINK_LIBRARIES(pylonbase pylonutility GenApi_gcc_v3_0_Basler_pylon_v5_0 GCBase_gcc_v3_0_Basler_pylon_v5_0 boost_python zmq pthread opencv_cudabgsegm opencv_cudaobjdetect opencv_cudastereo opencv_stitching opencv_cudafeatures2d opencv_superres opencv_cudacodec opencv_videostab opencv_cudaoptflow opencv_cudalegacy opencv_cudawarping opencv_aruco opencv_bgsegm opencv_bioinspired opencv_ccalib opencv_dpm opencv_freetype opencv_fuzzy opencv_hdf opencv_line_descriptor opencv_optflow opencv_reg opencv_saliency opencv_stereo opencv_structured_light opencv_phase_unwrapping opencv_rgbd opencv_surface_matching opencv_tracking opencv_datasets opencv_text opencv_face opencv_plot opencv_dnn opencv_xfeatures2d opencv_shape opencv_video opencv_ximgproc opencv_calib3d opencv_features2d opencv_flann opencv_xobjdetect opencv_objdetect opencv_ml opencv_xphoto opencv_highgui opencv_videoio opencv_photo opencv_imgcodecs opencv_cudaimgproc opencv_cudafilters opencv_imgproc opencv_cudaarithm opencv_core opencv_cudev)

add_executable(CameraDeamon ${SOURCE_FILES} ${OpenCV_LIBS} ${Pylon_LIBS})


