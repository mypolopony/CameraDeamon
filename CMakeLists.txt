cmake_minimum_required(VERSION 2.8.11)

project(CameraDeamon)



include_directories(
        /home/nvidia/CameraDeamon/CameraDeamon/
        /home/nvidia/CameraDeamon/lib
        /usr/include/lib
        /opt/pylon5/include
        /usr/local/include/bsoncxx/v_noabi
        /usr/local/include/mongocxx/v_noabi
        /usr/local/include/bsoncxx/v_noabi/bsoncxx/
        /home/nvidia/CameraDeamon/lib
        /usr/include/opencv2
        /usr/include
        /data/opencv_contrib/modules/xfeatures2d/include
        /data/CMake-hdf5-1.10.1/HDF_Group/HDF5/1.10.1/include

)

# Linker options
set(CMAKE_LDFLAGS "${CMAKE_LDFLAGS}  -pg -ggdb `pkg-config opencv --cflags --libs` `/opt/pylon5/bin/pylon-config --libs-rpath` `pkg-config --libs libmongocxx`") # /usr/lib/aarch64-linux-gnu/libz.so /usr/lib/aarch64-linux-gnu/libdl.so /usr/lib/aarch64-linux-gnu/libm.so")

# Library path
set(CMAKE_LDFLAGS "${CMAKE_LDFLAGS} -L\".\" -L\"/opt/pylon5/lib64\" -L\"/data/CMake-hdf5-1.10.1/HDF_Group/HDF5/1.10.1/lib\" -L\"/usr/local/lib64\" ")

# Define the CXX sources
set ( CXX_SRCS
        ../main.cpp
        ../AgriDataCamera.cpp
        ../AGDUtils.cpp
        ../lib/easylogging++.cc
        ../lib/json.hpp
        )

set_source_files_properties(
        ${CXX_SRCS} PROPERTIES COMPILE_FLAGS
        " -pg -O0 -w -std=c++11 -Wall -Wunknown-pragmas -ggdb")

find_package( OpenCV REQUIRED )

add_executable(CameraDeamon ${CXX_SRCS} ${C_SRCS})

target_link_libraries(CameraDeamon
        "/opt/pylon5/lib64/libpylonbase.so"
        "/opt/pylon5/lib64/libpylonutility.so"
        "/opt/pylon5/lib64/libGenApi_gcc_v3_0_Basler_pylon_v5_0.so"
        "/opt/pylon5/lib64/libGCBase_gcc_v3_0_Basler_pylon_v5_0.so"
        ${OpenCV_LIBS}
        mongocxx
        bsoncxx
        boost_system
        boost_filesystem
        boost_python
        zmq
        pthread
        profiler
        hdf5
        hdf5_hl
        hdf5_cpp
        ev
        )

