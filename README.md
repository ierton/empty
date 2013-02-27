Empty 
=====

By Mikhail Zakharov, Sergey Mironov and others, see README.old

With empty you can run applications under pseudo-terminal (PTY) sessions and
replace TCL/Expect with a simple tool under your favorite shell (sh, bash,
csh, tcsh, ksh, zsh, etc).

This is a version of original emtpy tool located at
[SourceForge](http://empty.sourceforge.net/).
The fork contains several bugfixes and improvements.  

Building
--------

To build the program, do the following.

	$ make
	$ sudo make install

Usage
-----

	empty [-d] -f [-i fifo1 -o fifo2] [-p file.pid] [-L file] command [command args]
	empty [-d] -w [-Sv] [-t n] [-i fifo2 -o fifo1] key1 [answer1] ... [keyX answerX]
	empty [-d] -s [-Sc] [-o fifo1] [request]
	empty [-d] -r [-b size] [-t n] [-i fifo1]
	empty [-d] -l
	empty [-d] -k [pid] [signal]
	empty -h


Bugs
----

Codebase is quite messy so they probably exist. Fortunately, this is a one-file
application so patches are welcome.

Sergey Mironov
ierton@gmail.com

