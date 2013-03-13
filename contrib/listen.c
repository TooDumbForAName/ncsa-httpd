/*
 * Startup program for netrek.  Listens for connections, then forks off
 * servers.  Based on code written by Brett McCoy, but heavily modified.
 *
 * To compile, first change the defines for SERVER, LOGFILE, DEF_PORT to what
 * you want.  Then use:
 *
 *      cc -O listen.c -o listen
 *
 * 'listen' only accepts one option, '-p' to override the default port to
 * listen on.  Use it like: 'listen -p 2592'.
 * 
 * Note that descriptor 2 is duped to descriptor 1, so that stdout and
 * stderr go to the same file.
 */



#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <netdb.h>
#include <errno.h>
#include <time.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <varargs.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>

#define SERVER		"/stg1/rob/netrek/lib/ntserv"	/* filename of server */
#define LOGFILE		"/dev/null"	/* filename of log file */
#define DEF_PORT	2592		/* port to listen on */

int listenSock;
short port = DEF_PORT;
char *program;

char *	dateTime();
void	detach();
void	getConnections();
void	getListenSock();
void	multClose();
void	reaper();
void	terminate();

/*
 * Error reporting functions ripped from my library.
 */
void syserr();
void warnerr();
void fatlerr();
void err();
char *lasterr();

struct hostent *	gethostbyaddr();
char *				inet_ntoa();



main(argc, argv)
int argc;
char *argv[];
{
	int i;

	for (i = 1; i < argc; i++) {

		if (argv[i][0] == '-') {
			if (strcmp(argv[i]+1, "p") == 0) {
				port = atoi(argv[i+1]);
			} else {
				fatlerr(1, 0, "unrecognized option: %s.", argv[i]);
			}
		}
	}

	program = argv[0];			/* let err functions know our name */
	detach();					/* detach from terminal, close files, etc. */
	getListenSock();
	signal(SIGCHLD, reaper);
	signal(SIGTERM, terminate);

	while (1) {
		getConnections();
	}
}

/***********************************************************************
 * Detach process in various ways.
 */

void
detach() {
    int fd, rc, mode;

	mode = S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR;
	if ((fd = open(LOGFILE, O_WRONLY|O_CREAT|O_APPEND, mode)) == -1)
		syserr(1, "detach", "couldn't open log file. [%s]", dateTime());
	dup2(fd, 2);
	dup2(fd, 1);
	multClose(1, 2, -1);	/* close all other file descriptors */
	warnerr(0, "started at %s on port %d.", dateTime(), port);

	/* fork once to escape the shells job control */
	if ((rc = fork()) > 0)
		exit(0);
	else if (rc < 0)
		syserr(1, "detach", "couldn't fork. [%s]", dateTime());

	/* now detach from the controlling terminal */
	if ((fd = open("/dev/tty", O_RDWR, 0)) == -1) {
		warnerr("detach", "couldn't open tty, assuming still okay. [%s]",
				dateTime());
		return;
	}
    ioctl(fd, TIOCNOTTY, 0);
    close(fd);
	setsid();	/* make us a new process group/session */
}

/***********************************************************************
 */

void
getListenSock() {
	struct sockaddr_in addr;
	
	if ((listenSock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
			syserr(1, "getListenSock", "can't create listen socket. [%s]",
				   dateTime());

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);

	if (bind(listenSock, (struct sockaddr *) &addr, sizeof(addr)) < 0)
		syserr(1, "getListenSock", "can't bind listen socket. [%s]",
			   dateTime());

	if (listen(listenSock, 5) != 0)
		syserr(1, "getListenSock", "can't listen to socket. [%s]", dateTime());
}

/***********************************************************************
 */

void
getConnections() {
	int len, sock, pid;
	struct sockaddr_in addr;
	struct hostent *he;
	char host[100];

	len = sizeof(addr);
	while ((sock = accept(listenSock, (struct sockaddr *) &addr, &len)) < 0) {
		/* if we got interrupted by a dying child, just try again */
		if (errno == EINTR)
			continue;
		else
			syserr(1, "getConnections",
				   "accept() on listen socket failed. [%s]", dateTime());
	}

	/* get the host name */
	he = gethostbyaddr((char *) &addr.sin_addr.s_addr,
					   sizeof(addr.sin_addr.s_addr), AF_INET);
	if (he != 0)
		strcpy(host, he->h_name);
	else
		strcpy(host, inet_ntoa((u_long) &addr.sin_addr));
	warnerr(0, "connection from %s. [%s]", host, dateTime());

	/* fork off a server */
	if ((pid = fork()) == 0) {
		dup2(sock, 0);
		multClose(0, 1, 2, -1);	/* close everything else */
		if (execl(SERVER, SERVER, host, 0) != 0)
			syserr(1, "getConnections", "couldn't execl %s as the server. [%s]",
				   SERVER, dateTime());
	} else if (pid < 0)
		syserr(1, "getConnections", "can't fork. [%s]", dateTime());

	close(sock);
}

/***********************************************************************
 * Returns a string containing the date and time.  String area is static
 * and reused.
 */
 
char *
dateTime() {
	time_t t;
	char *s;

	time(&t);
	s = ctime(&t);
	s[24] = '\0';	/* wipe-out the newline */
	return s;
}

/***********************************************************************
 * Handler for SIGTERM.  Closes and shutdowns everything.
 */

void
terminate() {
	int s;

	fatlerr(1, "terminate", "killed. [%s]", dateTime());

	/* shutdown and close everything */
	for (s = getdtablesize(); s >= 0; s--) {
		shutdown(s, 2);
		close(s);
	}
}

/***********************************************************************
 * Waits on zombie children.
 */

void
reaper() {
	while (wait3(0, WNOHANG, 0) > 0);
}

/***********************************************************************
 * Close all file descriptors except the ones specified in the argument list.
 * The list of file descriptors is terminated with -1 as the last arg.
 */

void
multClose(va_alist)
va_dcl
{
	va_list args;
	int fds[100], nfds, fd, ts, i, j;

	/* get all descriptors to be saved into the array fds */
	va_start(args);
	for (nfds = 0; nfds < 99; nfds++) {
		if ((fd = va_arg(args, int)) == -1)
			break;
		else
			fds[nfds] = fd;
	}

	ts = getdtablesize();

	/* close all descriptors, but first check the fds array to see if this
	 *  one is an exception */
	for (i = 0; i < ts; i++) {
		for (j = 0; j < nfds; j++)
			if (i == fds[j]) break;
		if (j == nfds) close(i);
	}
}

/***********************************************************************
 * Error reporting functions taken from my library.
 */

extern int sys_nerr;
extern char *sys_errlist[];
extern int errno;

void
syserr(va_alist)
va_dcl
{
	va_list args;
	int rc;

	va_start(args);
	rc = va_arg(args, int);
	err(args);
	if (errno < sys_nerr)
		fprintf(stderr, "     system message: %s\n", sys_errlist[errno]);

	exit(rc);
}

void
warnerr(va_alist)
va_dcl
{
	va_list args;

	va_start(args);
	err(args);
}

void
fatlerr(va_alist)
va_dcl
{
	va_list args;
	int rc;

	va_start(args);
	rc = va_arg(args, int);
	err(args);

	exit(rc);
}

void
err(args)
va_list args;
{
	
	char *func, *fmt;

	if (program != 0)
		fprintf(stderr, "%s", program);
	func = va_arg(args, char *);
	if (func != 0 && strcmp(func, "") != 0)
		fprintf(stderr, "(%s)", func);
	fprintf(stderr, ": ");

	fmt =  va_arg(args, char *);
	vfprintf(stderr, fmt, args);
	fputc('\n', stderr);
	fflush(stderr);
}

char *
lasterr() {
	if (errno < sys_nerr)
		return sys_errlist[errno];
	else
		return "No message text for this error.";
}

