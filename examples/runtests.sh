#!/bin/sh

echo "Starting endless tests"

while true; do
	for i in em_*; do
		echo "== Starting: $i ================================================" 
		./$i
		if [ $? != 0 ]; then
			echo "Error at test $i"
			exit 1
		fi
	done
done
