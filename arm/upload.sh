#!/bin/bash
if [[ "$1" == "opi" ]]; then
	rsync ../OrangePI/iot-hub root@192.168.1.22:/root/
	echo 'Deployed to Orange PI'
elif [[ "$1" == "rpi" ]]; then
	rsync ../RaspberryPI/iot-hub root@192.168.1.31:/root/
	echo 'Deployed to Raspberry PI'
else
	rsync ../OrangePI/iot-hub root@192.168.1.22:/root/
        echo 'Deployed to Orange PI'
        rsync ../RaspberryPI/iot-hub root@192.168.1.31:/root/
        echo 'Deployed to Raspberry PI'
fi