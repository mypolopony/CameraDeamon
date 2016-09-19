while true; do
	until /home/agridata/CameraDeamon/CameraDeamon/Debug/CameraDeamon; do
	    echo "Restarting CameraDeamon"
	    kill $(ps -A -ostat,ppid | awk '/[zZ]/{print $2}')
	    sleep 1
	done

	until /usr/bin/redis-server; do
	    echo "Restarting Redis"
            kill $(ps -A -ostat,ppid | awk '/[zZ]/{print $2}')
            sleep 1;
        done
done
