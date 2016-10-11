## Messages

The camera deamon is but one part of creating an interactive camera experience. The user should be able to send commands to the camera as well as receive current parameters and ideally a live-ish image. The [EmbeddedServer](https://github.com/motioniq/EmbeddedServer) is an HTTP server running on port 5000 that mediates these interactions. Although this service can be reached by any web interface (on the wireless network labeled _agridatahome_). There are also two beta / prototype iPhone applications that can, with some development, be used efficiently (see [xBasler](https://github.com/motioniq/xBasler) and [iOSBaslerController](https://github.com/motioniq/iOSBaslerController)).  

Issuances from the HTTP service are formed with a unique identifier, followed by a command, followed finally by a parameter if relevant. Because some of the Pylon / GenICam constants have names with underscores, a '-' delimiter is used to separate the three parts of the message. For example: 

<center>
`########-GainAuto-GainAuto_Once`
</center>

Note that there are some commands, like `GetStatus` and `Stop` that do not require a final parameter, although including one should not cause any undesired behavior.  

Return messages are formed slightly differently. Firstly, the delimiter is no longer so important. Secondly, a return value of 0 or 1 is included to indicate the success of the command. A successful response, for example, would look like:  

<center>
`########_1_GainAutoContinuous`
</center>

## Messenger

**ZMQ**, a distributed message service, is used to transfer, via socket on ports 5559 and 5558, the above commands and responses between the HTTP server and the CameraDeamon. On the Deamon side, zmq is non-blocking. The incoming message is parsed and the required action is performed.

## Status

One special command, `GetStatus`, is used as a kind of heartbeat. If asked for a status update, the Deamon will return with the current values of the camera, as well as an indication as to whether or not the camera is recording. It's good practice to ask for a status every few seconds in order to catch connection problems.