/* empty - run processes under pseudo-terminal sessions
 * 
 * Copyright (C) 2005-2009 Mikhail E. Zakharov
 * empty was written by Mikhail E. Zakharov. This software was based on the 
 * basic idea of pty version 4.0 Copyright (c) 1992, Daniel J. Bernstein, but
 * no code was ported from pty4.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions 
 *  are met:
 *  
 *  1. Redistributions of source code must retain the above copyright
 *     notice immediately at the beginning of the file, without modification,
 *     this list of conditions, and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 */

/* AIX and OSF1 code by Sylvain DEGUT (sylvain.degut@neuf.fr) 12/2006 */

#ifdef __SCO_VERSION__		/* We want SCO to look like the SVR4 system */
	#define __SVR4
#endif


#include <unistd.h>
#include <sys/types.h>

#if defined(__SVR4) || defined(__hpux__)
	#include <stropts.h>
#endif

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <termios.h>

#ifdef __FreeBSD__
	#include <libutil.h>
#endif

#ifdef __OpenBSD__
	#include <util.h>
	#define EIDRM EINVAL		/* Thanks "Elisender" <allex_9@ngs.ru> */
#endif

#if defined (__linux__) || defined (__CYGWIN__)
	#include <pty.h> 
	#include <utmp.h>
	#include <time.h>
	#include <sys/time.h>      
#endif

#ifdef __AIX
	#include <time.h>
	#include <sys/stropts.h>
	#include <sys/pty.h> 
	#include <utmp.h>
#endif

#ifdef __OSF1
	#include <sys/termios.h>
	#include <varargs.h>
#endif

#ifndef __hpux__
	#include <sys/select.h>
#endif

#if !defined(__SVR4) && !defined(__hpux__) && !defined(__AIX) && !defined(__OSF1)
	#include <err.h>
#endif

#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <syslog.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/param.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#include <regex.h>

#define tmpdir "/tmp"
#define program "empty"
#define version "0.6.19b" 

/* -------------------------------------------------------------------------- */
static void usage(void);
long toint(char *intstr);
void wait4child(int child, char *argv0);
int mfifo(char *fname, int mode);
void perrx(int ex_code, const char *err_text, ...);
void perrxslog(int ex_code, const char *err_text, ...);
long pidbyppid(pid_t ppid, int lflg);
void clean(void);
void fsignal(int sig);
int longargv(int argc, char *argv[]); 
int checkgr(int argc, char *argv[], char *buf, int chkonly);
int regmatch(const char *string, char *pattern, regex_t *re);
int watch4str(int ifd, int ofd, int argc, char *argv[],
		int Sflg, int vflg, int cflg, int timeout);
int parsestr(char *dst, char *src, int len, int Sflg);

/* -------------------------------------------------------------------------- */
int     master, slave;
int	child;
long pid = -1;
char	*in = NULL, *out = NULL, *sl = NULL, *pfile = NULL;
int	ifd, ofd, lfd = 0, pfd = 0;
FILE	*pf;
int	status;
char	buf[BUFSIZ];
fd_set	rfd;
char	*argv0 = NULL;
int	sem = -1;
struct sembuf free_sem = {0, 1, 0};

/* -------------------------------------------------------------------------- */
int main (int argc, char *argv[]) {
	struct	winsize win;
	struct	termios tt;
	int	i, bl, cc, n, ch;
	int	fflg = 0;		/* spawn, fork */
	int	wflg = 0;		/* watch for string [respond] */
	int	sflg = 0;		/* send */
	int	kflg = 0;		/* kill */
	int	lflg = 0;		/* list your jobs */
	int	iflg = 0;		/* in */
	int	oflg = 0;		/* out */
	int	Sflg = 0;		/* Strip last character from input */
	int	cflg = 0;		/* use stdin instead of FIFO */
	int	vflg = 0;		/* kvazi verbose mode OFF */
	int	timeout = 10;		/* wait N secs for the responce */
	int	Lflg = 0;		/* Log empty session */
	int	rflg = 0;		/* recv output */
	int	bflg = 0;		/* block size for -r flag */
	int	tflg = 0;		/* Timeout flag for -b (timeout?) */
	int	pflg = 0;		/* Shall we save PID to a file? */
	long	bs = 1;
	
	int	ksig = SIGTERM;
	
	pid_t	ppid;			/* Shell's PID */
	char	infifo[MAXPATHLEN];
	char	outfifo[MAXPATHLEN];

	struct sembuf check_sem = {0, -1, 0};
	key_t sem_key;
	const char *sem_file = "/";

	time_t	stime, ntime;
	struct	timeval tv;

	int	fl_state = 2;		/* 0 - in	>>>
					   1 - out	<<<
					   2 - unknown */
	
/* semaphores */
#ifdef _POSIX_SEMAPHORES
	#if defined(__linux__) && defined(__GNU_LIBRARY__) && !defined(_SEM_SEMUN_UNDEFINED)
		/* union semun is defined by including <sys/sem.h> */
	#else
		union semun {
			int val;
			struct semid_ds *buf;
		#ifdef __SVR4
			ushort_t	*array;
		#endif
		#ifdef __hpux__
			ushort		*array;
		#endif
		#ifdef __linux__
			unsigned short *array;
			struct seminfo *__buf;		/* buffer for IPC_INFO */
		#endif
		};
  	#endif
#endif
	union semun semu;
	
#if defined(__SVR4) || defined(__hpux__) || defined(__AIX)
	char	*slave_name;
	int	pgrp;
#endif

#ifndef __linux__
	while ((ch = getopt(argc, argv, "Scvhfrb:kwslp:i:o:t:L:")) != -1)
#else
	while ((ch = getopt(argc, argv, "+Scvhfrb:kwslp:i:o:t:L:")) != -1)
#endif
		switch (ch) {
			case 'f':
				fflg = 1;
				break;
			case 'k':
				/* Send signal */
				kflg = 1;
				break;
			case 'w':
				wflg = 1;
				break;
			case 's':
				sflg = 1;
				break;
			case 'l':
				lflg = 1;
				break;
			case 'i': 
				in = optarg;
				iflg = 1;
				break;
			case 'o': 
				out = optarg;
				oflg = 1;
				break;
			case 'p':
				pfile = optarg;
				pflg = 1;
				break;
			case 'r':
				rflg = 1;
				in = optarg;
				break;
			case 'b':
				bflg = 1;
				if ((bs = toint(optarg)) < 1) {
					fprintf(stderr,
						"Fatal: wrong -b value\n");
					(void)usage();
				}
				break;
			case 't':
				/* wait N secs for the responce. Use with -w, -r */
				tflg = 1;
				if ((timeout = (int)toint(optarg)) < 1) {
					fprintf(stderr,
						"Fatal: wrong -t value\n");
					(void)usage();
				}
				break;
			case 'L':
				/* Log session */
				sl = optarg;
				Lflg = 1;
				break;
			case 'c':
				/* use stdin instead of FIFO */
				cflg = 1;
				break;
		
			case 'S':
				/* Strip last character from input */
				Sflg = 1;
				break;
			case 'v':
				vflg = 1;
				break;
			case 'h':
			default:
				(void)usage();
		}
	argc -= optind;
	argv += optind;

	
	if ((fflg + kflg + wflg + sflg + lflg + rflg) != 1)
		(void)usage();

	ppid = getppid();
	
	if (kflg) {	/* kill PID with the SIGNAL */

		if (argv[0]) {
			if ((pid = toint(argv[0])) < 0) {
				fprintf(stderr, "Fatal: wrong PID value\n");
				(void)usage();
			}
	
			/* Signal */	
			if (argv[1]) 
				ksig = (int)toint(argv[1]);
		} else
			if ((pid = pidbyppid(ppid, lflg)) < 1)
				(void)perrx(255, "Can't find desired process");

		if (pid > 0 && (kill(pid, ksig) == -1))
			(void)perrx(255, "Can't kill PID: %d", pid);
	
		(void)exit(0);
	}

	if (lflg) {
		pidbyppid(ppid, lflg); 
		(void)exit(0);
	}

	if (sflg) {	/* we want to send */
		if (!oflg) {
			if ((pid = pidbyppid(ppid, lflg)) > 0) {
				snprintf(outfifo, sizeof(outfifo), "%s/%s.%ld.%ld.in",
					tmpdir, program, (long)ppid, (long)pid);
				out = (char *)outfifo;
			} else 
				(void)perrx(255, "Fatal can't find IN FIFO file by PPID:PID pair");
		}

		if ((ofd = open(out, O_WRONLY)) == -1)
			(void)perrx(255, "Fatal open FIFO for writing: %s", out);
		
		if (!cflg && argv[0] != NULL) {
			bl = parsestr(buf, argv[0], strlen(argv[0]), Sflg);
			if (write(ofd, buf, bl) == -1)
				(void)perrx(255, "Fatal write data to FIFO: %s", out);
		} else
			while ((cc  = read(0, buf, sizeof(buf))) > 0) {
				if (cc == -1)
					(void)perrx(255, "Fatal read from STDIN to buffer");
				bl = parsestr(buf, buf, cc, Sflg);
				if (write(ofd, buf, bl) == -1)
					(void)perrx(255, "Fatal write STDIN data to FIFO: %s", out);
			}

		(void)exit(0);
	}

	if (rflg) {
		if (!iflg) {
			if ((pid = pidbyppid(ppid, lflg)) > 0) {
				snprintf(infifo, sizeof(infifo), "%s/%s.%ld.%ld.out",
					tmpdir, program, (long)ppid, (long)pid);
				in = (char *)infifo;
			} else 
				(void)perrx(255, "Fatal can't find OUT FIFO file by PPID:PID pair");
		}

		if ((ifd = open(in, O_RDONLY)) == -1)
			(void)perrx(255, "Fatal open FIFO for reading: %s", in);


		FD_ZERO(&rfd);

		stime = time(0);
        	tv.tv_sec = timeout;
	        tv.tv_usec = 0;
	
		cc = -1; while (cc != 0) {
			FD_SET(ifd, &rfd);
			n = select(ifd + 1, &rfd, 0, 0, &tv);
#ifdef __linux__
			tv.tv_sec = timeout;	//fix because struct was set to 0; thanks "David Hofstee" <davidh@blinker.nl>
#endif
			if (n < 0 && errno != EINTR) 
				perrx(255, "Fatal select()");

			if (n > 0 && FD_ISSET(ifd, &rfd)) {
				if ((cc = read(ifd, buf, bs)) == -1)
					(void)perrx(255, "Fatal read from IN FIFO");
				if (write(1, buf, cc) == -1)
					(void)perrx(255, "Fatal write to STDOUT");
				if ((!bflg && buf[0] == '\n') || bflg)
					break;
			} else 
				if (tflg) {
					ntime = time(0);
					if ((ntime - stime) >= timeout) {
						(void)fprintf(stderr,
							"%s: Buffer is empty. Exit on timeout\n",
							program);
 
						(void)close(ifd);
						(void)exit(255);
				}
			}
		}
		
		(void)close(ifd);
		(void)exit(0);
	}

	if (argc == 0)
		(void)usage();

        /* Otpion -w in order to get keyphrases and send responses */
	if (wflg) {
		if (!iflg && !oflg) {
			if ((pid = pidbyppid(ppid, lflg)) > 0) {
				sprintf(infifo, "%s/%s.%ld.%ld.out",
					tmpdir, program, (long)ppid, (long)pid);
				sprintf(outfifo, "%s/%s.%ld.%ld.in",
					tmpdir, program, (long)ppid, (long)pid);
				in  = (char *)infifo;
				out = (char *)outfifo;
			} else
				(void)perrx(255, "Fatal can't find FIFO files by PPID:PID pair");
		}


		if ((ifd = open(in, O_RDONLY)) == -1)
			(void)perrx(255, "Fatal open FIFO for reading: %s", in);

		if ((ofd = open(out, O_WRONLY)) == -1)
			(void)perrx(255, "Fatal open FIFO for writing: %s", out);

		checkgr(argc, argv, NULL, 1);	/* check regexp syntax only */
		(void)exit(watch4str(ifd, ofd, argc, argv, Sflg, vflg, cflg, timeout));
	}


	(void)openlog(program, 0, 0);
	(void)syslog(LOG_NOTICE, "version %s started", version);
	argv0 = argv[0];
	
	(void)tcgetattr(STDIN_FILENO, &tt);
        (void)ioctl(STDIN_FILENO, TIOCGWINSZ, &win);

	if ((sem_key = ftok(sem_file, getpid())) == -1)
		(void)perrxslog(255, "Can't generate semaphore key from file %s %m", sem_file);
	
	if ((sem = semget(sem_key, 1, 0600 | IPC_CREAT)) == -1)
		(void)perrxslog(255, "Can't get semaphore %m");

	semu.val = 0;
	if (semctl(sem, 0, SETVAL, semu) == -1) 
		(void)perrxslog(255, "Can't set semaphore %d to lock: %m", sem);

	if ((pid = fork()) == -1)
		(void)perrxslog(255, "Daemonizing failed. Fatal first fork");
	if (pid > 0) {
		if ((semop(sem, &check_sem, 1) == -1) && (errno != EINVAL) && (errno != EIDRM))
			/* Semaphore was removed by the child */
			(void)perrxslog(255, "Can't get semaphore %d status: %m", sem);

		(void)exit(0);
	}
	
	if (setsid() < 0)
		(void)perrxslog(255, "Daemonizing failed. Fatal setsid");
	
	if ((pid = fork()) == -1)
		(void)perrxslog(255, "Daemonizing failed. Fatal second fork");
        /* Fork does not work properly */
	if (pid > 0) 
		(void)exit(0);
	pid = getpid();				/* MY PID */
	
	if (pflg) {
		if ((pfd = open(pfile, O_CREAT|O_WRONLY, S_IRWXU)) == -1)
			(void)syslog(LOG_NOTICE,
				     "Warning: Can't write pid-file %s %m", pfile);
		
		pf = fdopen(pfd, "w");
		fprintf(pf, "%ld\n", (long)pid);
		fclose(pf);
	}
	
	if (!(iflg && oflg)) {
		sprintf(infifo, "%s/%s.%ld.%ld.in", tmpdir, program, (long)ppid, (long)pid);
		sprintf(outfifo, "%s/%s.%ld.%ld.out", tmpdir, program, (long)ppid, (long)pid);
		in = (char *)infifo;
		out = (char *)outfifo;
	}

	if (Lflg)
		if ((lfd = open(sl, O_CREAT|O_WRONLY|O_APPEND, S_IRWXU)) == -1)
			(void)syslog(LOG_NOTICE,
				"Warning: Can't open %s for session-log %m", sl);

	if ((ifd = mfifo(in, O_RDWR)) == -1 && errno != ENXIO)
		(void)perrxslog(255, "Fatal creating FIFO: %s", in);
	if ((ofd = mfifo(out, O_RDWR)) == -1 && errno != ENXIO)
		(void)perrxslog(255, "Fatal creating FIFO: %s", out);
	
	if (semop(sem, &free_sem, 1) == -1)
		(void)perrxslog(255, "Can't release semaphore: %d from lock %m", sem);
	if (semctl(sem, 0, IPC_RMID) == -1)
		(void)syslog(LOG_NOTICE, "Warning: Can't remove semaphore: %d  %m", sem);

#if !defined(__SVR4) && !defined(__hpux__) && !defined(__AIX)
	if (openpty(&master, &slave, NULL, &tt, &win) == -1)
		(void)perrxslog(255, "PTY routine failed. Fatal openpty()");
#else
	#ifdef __AIX
		if ((master = open("/dev/ptc", O_RDWR | O_NOCTTY)) == -1)
			(void)perrxslog(255, "PTY routine failed. Fatal open(\"/dev/ptc\"), ...");
	#else
		if ((master = open("/dev/ptmx", O_RDWR)) == -1)
			(void)perrxslog(255, "PTY routine failed. Fatal open(\"/dev/ptmx\"), ...");
	#endif

	#ifdef __hpux__  
		/* See the same definition for Solaris & UW several lines below */
		if (grantpt(master) == -1)
			(void)perrxslog(255, "Can't grant access to slave part of PTY: %m");
	#endif
	
	if (unlockpt(master) == -1)
		(void)perrxslog(255, "PTY routine failed. Fatal unlockpt()");

	if ((slave_name = (char *)ptsname(master)) == NULL)
		(void)perrxslog(255, "PTY routine failed. Fatal ptsname(master)");

	#if defined(__SVR4) && !defined(__SCO_VERSION__)
		if (grantpt(master) == -1)
			(void)perrxslog(255, "Can't grant access to slave part of PTY: %m");
	#endif
#endif /* !defined(__SVR4) && !defined(__hpux__) && !defined(__AIX) */

	for (i = 1; i < 32; i++)
		signal(i, fsignal);	/* hook signals */

	if ((child = fork()) < 0) {
		(void)clean();
		(void)perrxslog(255, "Fatal fork at creating desired process");
	}

	/* If this is the main process : launch with -f */
	if (child == 0) {
		(void)close(master);

#if !defined(__SVR4) && !defined(__hpux__) && !defined(__AIX) 
		login_tty(slave);
	#ifndef __CYGWIN__
		cfmakeraw(&tt);
	#endif
#else
		if ((pgrp = setsid()) == -1)
			(void)syslog(LOG_NOTICE, "Warning: Can't setsid(): %m");

		if ((slave = open(slave_name, O_RDWR)) == -1)
			(void)perrxslog(255, "Fatal open slave part of PTY %s", slave_name);

	#ifndef __AIX	
		ioctl(slave, I_PUSH, "ptem");
		ioctl(slave, I_PUSH, "ldterm");
		ioctl(slave, I_PUSH, "ttcompat");
	#endif	
		/* Duplicate open file descriptor */
		dup2(slave, 0);
	       	dup2(slave, 1);
	       	dup2(slave, 2);

		/* Set foreground the main process */	 
		if (tcsetpgrp(0, pgrp) == -1) 
	  	  (void)perrxslog(255, "Fatal tcsetpgrp()");

#endif

#if defined(__SVR4)
		/* Setup terminal parameters for Solaris to work under cron or java.
		Thanks to "lang qiu" <qiulang@gmail.com> for the initial line of parameters */
		tt.c_lflag = ISIG | ICANON | ECHOE | ECHOK | ECHOCTL | ECHOKE | IEXTEN; 
		tt.c_oflag = TABDLY | OPOST;
		tt.c_iflag = BRKINT | IGNPAR | ISTRIP | ICRNL | IXON | IMAXBEL;
		tt.c_cflag = CBAUD | CS8 | CREAD; 
#endif
		tt.c_lflag &= ~ECHO; 
		(void)tcsetattr(STDIN_FILENO, TCSAFLUSH, &tt);

		strncpy(buf, argv[0], sizeof(buf) - 1);
		buf[sizeof(buf) - 1] = '\0';

		for (i = 1; i < argc; i++) {
			strncat(buf, " ", 1);
			strncat(buf, argv[i], sizeof(buf));
		}

		(void)syslog(LOG_NOTICE, "forked %s", buf);

		/* Exec and quit if successful */
 		execvp(argv[0], argv);

		(void)syslog(LOG_NOTICE, "Failed loading %s: %m", buf);

		(void)clean();
		(void)kill(0, SIGTERM);
		(void)exit(0);
	}

	/* If this is the forked process */
	FD_ZERO(&rfd);	
	for (;;) {
		FD_SET(master, &rfd);
		FD_SET(ifd, &rfd);
	
		n = select(master + 1, &rfd, 0, 0, NULL);
		if (n > 0 || errno == EINTR) {
			if (FD_ISSET(ifd, &rfd)) 
				if ((cc = read(ifd, buf, sizeof(buf))) > 0) {
					/* our input */
					(void)write(master, buf, cc);
					if (lfd) {
						if (fl_state != 1) {
							(void)write(lfd, ">>>", 3);
							fl_state = 1;
						}
						(void)write(lfd, buf, cc);
					}
				}

			if (FD_ISSET(master, &rfd))
				if ((cc = read(master, buf, sizeof(buf))) > 0) {
					/* remote output */
					(void)write(ofd, buf, cc);
					if (lfd) {
						if (fl_state != 0) {
							(void)write(lfd, "<<<", 3);
							fl_state = 0;
						}
						(void)write(lfd, buf, cc);
					}
				}
		}
	}
/* 	This never reached */
/*	return 0; */
}

/* -------------------------------------------------------------------------- */
static void usage(void) {
	(void)fprintf(stderr,
"%s-%s usage:\n\
empty -f [-i fifo1 -o fifo2] [-p file.pid] [-L file] command [command args]\n\
empty -w [-Sv] [-t n] [-i fifo2 -o fifo1] key1 [answer1] ... [keyX answerX]\n\
empty -s [-Sc] [-o fifo1] [request]\n\
empty -r [-b size] [-t n] [-i fifo1]\n\
empty -l\n\
empty -k [pid] [signal]\n\
empty -h\n", program, version);
	exit(255);
}

/* -------------------------------------------------------------------------- */
long toint(char *intstr) {
	int	in;

	in = strtol(intstr, (char **)NULL, 10);
	if (in == 0 && errno == EINVAL) {
		fprintf(stderr, "Wrong long value: %s\n", intstr);
		usage();
	}
	return in;
}

/* -------------------------------------------------------------------------- */
long pidbyppid(pid_t ppid, int lflg) {
	char fmask[MAXPATHLEN];
	char fname[MAXPATHLEN];
	const char *sep = ".";
	DIR *dir;
	struct dirent *dent;
	int len;
	char *chpid, *tail;
	long pid = -1, maxpid = -1;
	int header = 1;

	/* form this line: empty.ppid */
	sprintf(fmask, "%s%s%d", program, sep, ppid);
	len = strlen(fmask);

	if ((dir = opendir(tmpdir)) == NULL)
		(void)perrx(255, "Can't open directory: %s", tmpdir);

	while ((dent = readdir(dir)) != NULL) {
		if (!strncmp(fmask, dent->d_name, len)) {
			strncpy(fname, dent->d_name, sizeof(fname) - 1);
			fname[sizeof(buf) - 1] = '\0';

			strtok(fname, sep);		/* empty */
			strtok(NULL, sep);		/* PPID */
			chpid = strtok(NULL, sep);	/* PID */
			tail = strtok(NULL, sep);	/* IN or OUT */

			if (chpid != NULL) {
				pid = toint(chpid);
				pid > maxpid ? maxpid = pid : pid;
			}

			if (lflg) {
				if (header) {
					printf("PPID\tPID\tTYPE\tFILENAME\n");
					header --;
				}
					
				printf("%ld\t%ld\t%s\t%s/%s\n",
					(long)ppid, (long)pid, tail, tmpdir, dent->d_name);
			}
		}

	}

	if (closedir(dir) == -1)
		(void)perror("Warning closing directory");

	
	if (lflg && pid > 0)
		printf("\n%ld\t%ld\tcurrent\n", (long)ppid, (long)maxpid);

	return maxpid;
}

/* -------------------------------------------------------------------------- */
void wait4child(int child, char *argv0) {
	while ((pid = wait3(&status, WNOHANG, 0)) > 0)
		if (pid == child)  
			(void)syslog(LOG_NOTICE, "%s exited", argv0);
}	

/* -------------------------------------------------------------------------- */
int mfifo(char *fname, int mode) {
   
   if (mkfifo(fname, S_IFIFO|S_IRWXU) == -1)
          return -1;

   return open(fname, mode);

}

/* -------------------------------------------------------------------------- */
void clean(void) {
	(void)close(master);
       	(void)close(ifd);
       	(void)close(ofd);
       	(void)close(lfd);
	(void)unlink(in);
	(void)unlink(out);
	(void)unlink(pfile);
}		

/* -------------------------------------------------------------------------- */
void perrx(int ex_code, const char *err_text, ...) {
	char err_buf[BUFSIZ];
	va_list	va;

	va_start(va, err_text);
	vsnprintf(err_buf, sizeof(err_buf), err_text, va);
	va_end(va);
	 
	(void)perror(err_buf);
	(void)exit(ex_code);
}

/* -------------------------------------------------------------------------- */
void perrxslog(int ex_code, const char *err_text, ...) {
	va_list va;
	
	va_start(va, err_text);
#if !defined(__hpux__) && !defined(__AIX) && !defined(__OSF1)
	(void)vsyslog(LOG_NOTICE, err_text, va);
#else
	char err_buf[BUFSIZ];

	vsprintf(err_buf, err_text, va);
	(void)syslog(LOG_NOTICE, err_buf, "");
#endif
	
	va_end(va);
	
	if (sem != -1)
		semctl(sem, 0, IPC_RMID);

	(void)closelog();
	(void)exit(ex_code);
}

/* -------------------------------------------------------------------------- */
void fsignal(int sig) {
	switch(sig) {
		case SIGTERM:
		case SIGINT:
		case SIGQUIT:
		case SIGSEGV:
			break;
		case SIGCHLD:
			wait4child(child, argv0);
			(void)syslog(LOG_NOTICE,
				"version %s finished", version);
	}

	(void)clean();
	semop(sem, &free_sem, 1);
	(void)closelog();
	(void)exit(0);
}

/* -------------------------------------------------------------------------- */
int longargv(int argc, char *argv[]) {
	int i = 0, len = 0, maxlen = 0;


	while (argv[i] != NULL) {
		len = strlen(argv[i]);
		if (len > maxlen)
			maxlen = len;
		i++;
	}
	
	return maxlen;
}

/* -------------------------------------------------------------------------- */
int checkgr(int argc, char *argv[], char *buf, int chkonly) {
	int	i;
	regex_t re;

	for (i = 1; i <= argc; i++) {
		if (regcomp(&re, argv[i - 1], REG_EXTENDED | REG_NOSUB) != 0)
			(void)perrx(255, "Regex compilation failed");
	
		if (chkonly != 1)	
			switch (regmatch(buf, argv[i - 1], &re)) {
				case 1:	/* match */
					return i;
				case 0:	/* not found, check next key */
					if ((i + 1) <= argc)
						i++;
					break;
			}
	}

	return 0;	/* nothing found */
}

/* -------------------------------------------------------------------------- */
int regmatch(const char *string, char *pattern, regex_t *re) {
	int	status;

	/* regcomp() is not needed as it was previously executed by checkgr() */
	status = regexec(re, string, (size_t) 0, NULL, 0);
	regfree(re);

	switch (status) {
		case 0:
			return(1);

		case REG_NOMATCH:
			return(0);

		default:
			(void)perrx(255, "Regex execution failed");	
	}

 	return(255);	/* Not reached */
}

/* -------------------------------------------------------------------------- */
int watch4str(int ifd, int ofd, int argc, char *argv[], 
		int Sflg, int vflg, int cflg, int timeout) {

	int	n, cc, bl;
	time_t	stime, ntime;
	struct	timeval tv;

	int	argt = 0;
	int bread = 0;
	char	*resp = NULL;
	int found;
	int i;


	stime = time(0);
	tv.tv_sec = timeout;
	tv.tv_usec = 0;

	FD_ZERO(&rfd);
	for (;;) {
		FD_SET(ifd, &rfd);
		n = select(ifd + 1, &rfd, 0, 0, &tv);
#ifdef __linux__
		tv.tv_sec = timeout; //fix because struct was set to 0; thanks David Hofstee" <davidh@blinker.nl>
#endif
		if (n < 0 && errno != EINTR) 
			perrx(255, "Fatal select()");

		if (n > 0 && FD_ISSET(ifd, &rfd)) {
			if ((cc = read(ifd, buf + bread, sizeof(buf)-bread-1)) > 0) {
				stime = time(0);
				
				buf[cc + bread] = '\0';

				if (vflg)
					(void)printf("%s", buf);

				if ((argt = checkgr(argc, argv, buf, 0)) > 0) {
					if ((resp = argv[argt])) {
						/* Nb chars for buf */
						bl = parsestr(buf, resp, strlen(resp), Sflg);
						/* write response to fifo */
						if (write(ofd, buf, bl) == -1) 
						    (void)perrx(255, "Fatal write data to output");
					}
					/* exit program */
					return (argt + 1) / 2;
				}

				for(found=0,i=bread; i<bread+cc; i++) {
					if(buf[i] == '\n') {
						memmove(buf, buf+i+1, bread+cc-i-1);
						bread = bread+cc-i-1;
						found = 1;
						break;
					}
				}

				if(!found) 
					bread += cc;
			}

			if (cc <= 0) {
				/* Got EOF or ERROR */
				if (vflg)
					(void)fprintf(stderr, "%s: Got nothing in output\n",
						program);
				return 255;
			}
		}

		ntime = time(0);
		if ((ntime - stime) >= timeout) {
			(void)fprintf(stderr,
				"%s: Data stream is empty. Keyphrase wasn't found. Exit on timeout\n",
				program);
			return 255;
		}
	}
}

/* -------------------------------------------------------------------------- */
int parsestr(char *dst, char *src, int len, int Sflg) {
	int i, bi;
	
	/* Return numbers of chars for response */
	Sflg == 1 ? len-- : len;
	for (i = 0, bi = 0; i < len; i++, bi++) {
		if (src[i] == '\\')
			switch (src[i + 1]) {
				case '\\':
					dst[bi] = '\\';
					i++;
					break;
				case 'n':
					dst[bi] = '\n';
					i++;
					break;
				case 'r':
					dst[bi] = '\r';
					i++;
					break;
				default:
					dst[bi] = src[i];
			}
		else
			dst[bi] = src[i];
	}

	return bi;
}

/* -------------------------------------------------------------------------- */

