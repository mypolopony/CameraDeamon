#!/bin/bash

SECS=${1:-9999}
make
echo "Killing"
ps aux  |  grep -i EmbeddedServer  |  awk '{print $2}'  |  xargs sudo kill -9
ps aux  |  grep -i CameraDeamon  |  awk '{print $2}'  |  xargs sudo kill -9

echo "Starting Server"
/home/nvidia/EmbeddedServer/env/bin/python $HOME/EmbeddedServer/server.py & 
SERVER=$!
echo "Starting Camera Deamon"
$HOME/CameraDeamon/CameraDeamon/Debug/CameraDeamon &
CAMERA=$!
echo "Sleeping for 10"
sleep 10
START=1
END=$SECS
echo "Initiating scan ($END seconds)"
http http://localhost:5000/start
for (( c=$START; c<=$END; c++ ))
do	
	echo "$c of $END" 
	# http http://localhost:5000/live-data
    sleep 1
done 

echo "Killing"
http http://localhost:5000/stop
sleep 6
kill $SERVER
kill $CAMERA
