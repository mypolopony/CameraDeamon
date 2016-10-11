## Basic Camera Operation

In a default configuration, the camera is easily recognized and initialized for continuous grabbing. Based on the target FPS, _camera.RetrieveResult(...)_ will return a smart pointer which gives access to the bitwise image representation. 

## Video Format

It's important to distinguish between the _codec_ used to encode the video and the _container_ used to house it. We choose MPEG as the codec and AVI as the container. This has been well-suited for us because the compression is minimal (?) and because frames in AVI do not depend on what has come before or what will come after, essentially producing a frame-by-frame slideshow. Indeed, from these videos, frame images are easily extracted (using ffmpeg, for example).

## Heartbeats

The Deamon has an internal heartbeat as well as certain events which are set to occur sometime during hte beat. These are set at the beginning of the _run(...)_ method. Currently, the counters are illogically coded, but these were kept for the sake of stability. There are checkpoints to:
- output a compressed image for immediate viewing
- write to the frame log
- close and reopen a movie file (to keep files < 3GB)

## Threads

Threads are essential for many reasons, but primarily because the driver has to accomodate incoming images (1280x1024) at a rate of 20/sec. The image format is CV_8UC3, i.e. 8-bit unsigned with three channels. The filesize, then, is 24 * 1280 * 1024 bits, or somewhere near 3.9MB for one image, or 78MB per second. Luckily, USB3.0 is rated at 5Gbit/s (625 MB/s).

Having said that, it is best for the frame grabbing methods to do as little as possible, i.e. only grab frames. If the main thread is busy when it is supposed to be obtaining new frames, we can experience de-synchronization and frame dropping. Thus, for example, great advances and reducing frame loss were gained by moving the streaming image to a deparate frame. As we will see in a MultiCam setup, this becomes increasingly important.

## Camera Parameters

Some settings, like those for exposure and gain modes, are defined initially. Those, and others, can also be changed mid-recording. Pylon's SDK makes accessing these properties slightly awkward, as they are all integers masked as text-based constants (ExposureAuto_Once or GainAuto_Continuous, for example).

Initial settings currently:
- FPS to 20
- Exposure Lower Limit to 61
- Exposure Upper Limit to 1200
- Auto Function Profile set to Minimize Exposure Time
- Gain set to (auto) Continuous
- Exposure set to (Auto) Continuous