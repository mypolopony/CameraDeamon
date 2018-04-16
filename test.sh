#!/bin/bash

echo "Making"
make

echo "Restarting"
sudo systemctl restart camerastack

echo "Waiting 10 seconds"
sleep 10

echo "Disable annoying IMU"
sudo systemctl stop imu

echo "Recording"
http http://localhost/start

echo ". . . for 10"
sleep 10

echo "Stopping"
http http://localhost/stop
