#!/bin/sh

# emssh.sh v.0.5
# ssh example for empty
# Though, don't really know who wants to use empty for ssh :)
# Copyright (C) 2005, 2006 Mikhail E. Zakharov
#

ssh="ssh"				# (/full/path/to/)ssh
target="localhost"			# target host
login="luser"				# username (Change it!)
password="TopSecret"			# password (Change it!)

fifo_in="/tmp/empty.in"			# input fifo
fifo_out="/tmp/empty.out"		# output

# -----------------------------------------------------------------------------
cmd="$ssh $login@$target"
tmp="/tmp/empty.tmp"			# tempfile to store results

echo "Starting empty"
empty -f -L $tmp -i $fifo_in -o $fifo_out $cmd
if [ $? = 0 ]; then
	if [ -w $fifo_in -a -r $fifo_out ]; then
		echo "Sending Password"
		empty -w -v -i $fifo_out -o $fifo_in -t 5 assword: "$password\n"
		echo "Sending tests"
		empty -s -o $fifo_in "echo \"-- EMPTY TEST BEGIN --\"\n"
		empty -s -o $fifo_in "uname -a\n"
		empty -s -o $fifo_in "uptime\n"
		empty -s -o $fifo_in "who am i\n"
		empty -s -o $fifo_in "echo \"-- EMPTY TEST END --\"\n"
		echo "Sending exit"
		empty -s -o $fifo_in 'exit\n'
		echo "Check results:"
		sleep 1
		cat $tmp
		rm -f $tmp
	else
		echo "Error: Can't find I/O fifos!"
		return 1
	fi
else
	echo "Error: Can't start empty in daemon mode"
	return 1
fi

echo "Done"
