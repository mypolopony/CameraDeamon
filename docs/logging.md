## Logging Server

There are several approaches to logging, especially when we must consider that we will have different threads that need to access the same log destination. In theory, ZMQ or a similar socket-based messaging service can be used to send errors and info to a logging server. There are benefits to this, like allowing the user to investigate the logs in real time via the web or a mobile application. 

## System Logger 

On the other hand, it is helpful to delegate the system logger to handle logging messages from the Deamon. This is the solution we took. Xenia uses _rsyslog_ which, after modifying the configuration settings, sends messages to agridata.log -- in this way, all messages can be generically and robustly intercepted. Also, _rsyslog_ allows for log preservation, compression, and rotation. The C++ interface is provided by `<syslog.h>`.