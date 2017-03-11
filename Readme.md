# CameraD(ae)mon

Documentation: http://motioniq.github.io/CameraDeamon

# To profile
- get gproftools: https://github.com/gperftools/gperftools (requires compiling http://download.savannah.gnu.org/releases/libunwind/libunwind-0.99-beta.tar.gz from source, which in turn requires a special `-U_FORTIFY_SOURCE` to gcc flags, but only for one object in the compilation)

- compile program with `-lprofiler` from /usr/lib (`-g` is also necessary)

- catch zmq errors (check out comments in code)

- execute via `LD_PRELOAD=/usr/lib/libprofiler.so CPUPROFILE=/tmp/profile.out CameraDeamon/Debug/CameraDeamon`

- create a text profile: `pprof --text /home/agridata/CameraDeamon/CameraDeamon/Debug/CameraDeamon /tmp/profile.out > profiles/short/short.txt`

- or use the web version to create a call graph .svg
