#!/bin/sh

# emsu.sh v.0.4
# su example for empty
# Copyright (C) 2005, 2006 Mikhail E. Zakharov
#

password="RootPassword"			# password (Change it!)

fifo_in="/tmp/empty.in"			# input fifo
fifo_out="/tmp/empty.out"		# output

# -----------------------------------------------------------------------------
tmp="/tmp/empty.tmp"			# tempfile to store results

echo "Starting empty"
empty -f -i $fifo_in -o $fifo_out su 
if [ $? = 0 ]; then
	sleep 1			# heh, we may need this sleep() again 
	if [ -w $fifo_in -a -r $fifo_out ]; then
		echo "Sending Password"
		empty -w -v -i $fifo_out -o $fifo_in -t 5 assword: "$password\n"
		echo "Sending tests"
		empty -s -o $fifo_in "echo EMPTY TEST BEGIN\n > $tmp"
		empty -s -o $fifo_in "who am i\n >> $tmp"
		empty -s -o $fifo_in "id\n >> $tmp"
		empty -s -o $fifo_in "echo EMPTY TEST END\n >> $tmp"
		echo "Sending exit"
		empty -s -o $fifo_in 'exit\n'
		echo "Check results:"
		sleep 1
		cat $tmp
	else
		echo "Error: Can't find I/O fifos!"
		return 1
	fi
else
	echo "Error: Can't start empty in daemon mode"
	return 1
fi

echo "Done"
