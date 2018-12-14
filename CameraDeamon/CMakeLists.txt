cmake_minimum_required(VERSION 2.8.11)

project(CameraDeamon)



include_directories(
    /data/CameraDeamon/CameraDeamon/
    /data/CameraDeamon/CameraDeamon/lib
    /usr/include/lib
    /opt/pylon5/include
    /usr/local/include/bsoncxx/v_noabi
    /usr/local/include/mongocxx/v_noabi
    /usr/local/include/bsoncxx/v_noabi/bsoncxx/
    /data/CameraDeamon/lib
    /usr/include/opencv2
    /usr/include
    /data/opencv_contrib/modules/xfeatures2d/include
    /data/CMake-hdf5-1.10.1/HDF_Group/HDF5/1.10.1/include

)

# Linker options
set(CMAKE_LDFLAGS "${CMAKE_LDFLAGS}  -pg -ggdb `pkg-config opencv --cflags --libs` `/opt/pylon5/bin/pylon-config --libs-rpath` `pkg-config --libs libmongocxx` /usr/lib/aarch64-linux-gnu/libz.so /usr/lib/aarch64-linux-gnu/libdl.so /usr/lib/aarch64-linux-gnu/libm.so")

# Library path
set(CMAKE_LDFLAGS "${CMAKE_LDFLAGS} -L\".\" -L\"/opt/pylon5/lib64\" -L\"/usr/local/lib64\" -L\"/data/CMake-hdf5-1.10.1/HDF_Group/HDF5/1.10.1/lib\" ")

# Define the CXX sources
set ( CXX_SRCS
    ../main.cpp
    ../AgriDataCamera.cpp
    ../AGDUtils.cpp
    ../lib/easylogging++.cc
)

set_source_files_properties(
    ${CXX_SRCS} PROPERTIES COMPILE_FLAGS 
    " -pg -O0 -w -std=c++11 -Wall -Wunknown-pragmas -ggdb")

add_executable(CameraDeamon ${CXX_SRCS} ${C_SRCS})

target_link_libraries(CameraDeamon
    pylonbase
    pylonutility
    GenApi_gcc_v3_0_Basler_pylon_v5_0
    GCBase_gcc_v3_0_Basler_pylon_v5_0
    boost_system
    boost_filesystem
    boost_python
    zmq
    pthread
    profiler
    hdf5
    hdf5_hl
    redox
    ev
    hiredis
)

