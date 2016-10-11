## MultiCam

Several experiments were conducted to implement more than one camera. Although Selwyn-Lloyd's Ubuntu box is a good testbed, there may be aspects of this hardware setup that simply cannot be done without access to an actual box.  

Basler has a reasonable interface to assign cameras to an array that allows the frame grabbing to be handled internally. Each camera is well-identified by its DeviceSerialNumber and Pylon works hard to distribute the frames to their required destination, after which they can be handled as usual. Ideally, this setup will work (see `cams.cpp`) though it has not been entirely integrated.  

The [MultiCam](https://github.com/motioniq/CameraDeamon/tree/MultiCam) thread of the CameraDeamon repository contains the active development. Currently, although it has not been tested, this version of the CameraDeamon will accept more than one camera, but it does not use Pylon's internal management scheme. Instead, it is a blunt, threaded version that simply isolates each camera, allowing each to work simultaneously without interfering with one another.