cmake_minimum_required(VERSION 2.8)

project(hdf5demo CXX)

set(CMAKE_BUILD_TYPE Debug) 
set(CMAKE_CXX_STANDARD 11)

find_package(OpenCV REQUIRED)
message(STATUS "OpenCV library status:")
message(STATUS "    version: ${OpenCV_VERSION}")
message(STATUS "    libs: ${OpenCV_LIBS}")
message(STATUS "    libraries: ${OpenCV_LIBRARIES}")
message(STATUS "    include path: ${OpenCV_INCLUDE_DIRS}")

find_package(HDF5 REQUIRED)
message(STATUS "HDF5 library status:")
message(STATUS "    version: ${HDF5_VERSION}")
message(STATUS "    libs: ${HDF5_LIBS}")
message(STATUS "    libraries: ${HDF5_LIBRARIES}")
message(STATUS "    include path: ${HDF5_INCLUDE_DIRS}")

add_executable(hdf5demo ex_image1.cpp)

set(the_description "Hierarchical Data Format I/O")
link_directories("/usr/lib/aarch64-linux-gnu" "/data/opencv/build/lib")
include_directories(hdf5demo ${OpenCV_INCLUDE_DIRS} ${HDF5_INCLUDE_DIRS} "/usr/include" "/data/opencv_contrib/modules/hdf/include")
target_link_libraries(hdf5demo ${HDF5_LIBRARIES} ${OpenCV_LIBS} "/data/opencv/build/lib/libopencv_hdf.so" hdf5 hdf5_hl)

