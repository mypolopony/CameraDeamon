while true; do
	until /home/agridata/CameraDeamon/CameraDeamon/Debug/CameraDeamon; do
	    echo "Restarting"
	    kill $(ps -A -ostat,ppid | awk '/[zZ]/{print $2}')
	    sleep 1
	done
done
