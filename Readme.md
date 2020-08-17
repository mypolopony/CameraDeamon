# CameraD(ae)mon

This project is a driver for an arbitrary number of Basler GigE Cameras.

### Messaging
Non-blocking message handling between the cameras, the driver, and the user are accomplished with ZeroMQ [https://zeromq.org/] over TCP. The control service (_main_) subscribes on port 4999 and publishes on port 4998. The driver, _AgriDataCamera_ contains a client listening on 4997.

### Resiliency
The use-case expects the cameras to be started and stopped via web interface or command line, but also requires that the cameras stop and start as gracefully as possible during loss of power and potential reboot.

### User Interaction
The system, or user, can request status messages on the cameras at any time whether recording or not. The frame rate can be changed by the user or automatically in the case of detected frame loss. Image qualities like ROI, white balance, gain, exposure, and luminance can be changed on the fly. These settings can be made to persist or can reset when the cameras start a new recording.

### Imagery
The CMOS Basler acA1920-40gc GigE camera delivers 42 frames per second at 2.3 MP (1920 px x 1200 px) resolution. With two cameras, 20 frames per second is possible. The image size is about 6.3mb, so 2*6.3*20 ~= 250mb per second during recording. This is far too much to analyze and store, so for each frame, we: 

- Resize to any target size
- Conversion from BGR8Packed to RGB JPG (buffer)
- Save inside HDF5 file (one per minute)
- Write thumbnail to disk (for the user app to view the stream live)

All of this occurs in a different thread than the recording and does cause frame loss.

### Database
MongoDB is used. Metadata for each frame, most importantly timestamp, is recorded. Additionally, each recording session is logged to that database. The 'scan' contains all metadata related to the recording session, including input from the user app.

### Bandwidth
There is a large discussion on maintaining an optimal frame rate and even with only two cameras, packet loss is difficult to manage. It takes some tuning of the packet size and inter-packet delay that will depend on your specific system. Frame rate is throttled automatically if frame loss becomes a problem

### To profile
- get gproftools: https://github.com/gperftools/gperftools (requires compiling http://download.savannah.gnu.org/releases/libunwind/libunwind-0.99-beta.tar.gz from source, which in turn requires a special `-U_FORTIFY_SOURCE` to gcc flags, but only for one object in the compilation)
- compile program with `-lprofiler` from /usr/lib (`-g` is also necessary)
- catch zmq errors (check out comments in code)
- create a text profile: `pprof --text /home/agridata/CameraDeamon/CameraDeamon/Debug/CameraDeamon /tmp/profile.out > profiles/short/short.txt`
- or use the web version to create a call graph .svg
