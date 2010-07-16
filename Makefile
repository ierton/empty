#
# Makefile for empty
#
# Usage:
# 	make all install clean
# or
# 	make `uname -s` install clean
# or 
# 	make `uname -s`-gcc install clean
#


CC =	gcc
LIBS =	-lutil

PREFIX = /usr/local

all:
	${CC} ${CFLAGS} -Wall ${LIBS} -o empty empty.c

FreeBSD:	all
NetBSD:		all
OpenBSD:	all

Linux:		all
Cygwin:		all

UnixWare:	SunOS
OpenUNIX:	SunOS
AIX:		SunOS
OSF1:		SunOS
HP-UX:		SunOS
SunOS:
	cc -o empty empty.c

UnixWare-gcc:	SunOS-gcc
OpenUNIX-gcc:	SunOS-gcc
HP-UX-gcc:	SunOS-gcc
SunOS-gcc:
	gcc ${CFLAGS} -Wall -o empty empty.c

install:
	[ -f `which strip` ] && strip empty
	[ -d ${PREFIX}/bin ] && cp empty ${PREFIX}/bin || mkdir -p ${PREFIX}/bin && cp empty ${PREFIX}/bin
	[ -d ${PREFIX}/man/man1 ] && cp empty.1 ${PREFIX}/man/man1 || mkdir -p ${PREFIX}/man/man1 && cp empty.1 ${PREFIX}/man/man1
deinstall:
	rm ${PREFIX}/bin/empty
	rm ${PREFIX}/man/man1/empty.1
uninstall:	deinstall

clean:
	rm empty 
