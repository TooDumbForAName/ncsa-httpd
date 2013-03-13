/************************************************************************
 * NCSA HTTPd Server
 * Software Development Group
 * National Center for Supercomputing Applications
 * University of Illinois at Urbana-Champaign
 * 605 E. Springfield, Champaign, IL 61820
 * httpd@ncsa.uiuc.edu
 *
 * Copyright  (C)  1995, Board of Trustees of the University of Illinois
 *
 ************************************************************************
 *
 * portability.h,v 1.32 1996/03/27 20:44:29 blong Exp
 *
 ************************************************************************
 *
 */


#ifndef _PORTABILITY_H_
#define _PORTABILITY_H_

/* Define one of these according to your system. */
#if defined(SUNOS4)
#define BSD
#undef NO_KILLPG
#undef NO_SETSID
#define FD_BSD
#define JMP_BUF sigjmp_buf
char *crypt(char *pw, char *salt);
#define DIR_FILENO(p)  ((p)->dd_fd)

#elif defined(SOLARIS2)
#undef BSD
#define NO_KILLPG
#undef NO_SETSID
#define FD_BSD
#define MIX_SOCKADDR
#define bzero(a,b) memset(a,0,b)
#define JMP_BUF sigjmp_buf
#define DIR_FILENO(p)  ((p)->dd_fd)
#define NEED_CRYPT_H

#elif defined(IRIX)
#undef BSD
#undef NO_KILLPG
#undef NO_SETSID
#define FD_BSD
#define JMP_BUF sigjmp_buf
#define HEAD_CRYPT

#elif defined(HPUX)
#undef BSD
#define NO_KILLPG
#undef NO_SETSID
#define FD_BSD
#ifndef _HPUX_SOURCE
# define _HPUX_SOURCE
#endif /* _HPUX_SOURCE */
#define JMP_BUF sigjmp_buf
#define DIR_FILENO(p)  ((p)->dd_fd)

#elif defined(AIX3)
#undef BSD
#undef NO_KILLPG
#undef NO_SETSID
#define FD_BSD
#define NEED_SELECT_H
#define JMP_BUF sigjmp_buf
#define DIR_FILENO(p)  ((p)->dd_fd)
#define HEAD_CRYPT
#define MISSING_HEADERS

#elif defined(AIX4)
#undef BSD
#undef NO_KILLPG
#undef NO_SETSID
#define FD_BSD
#define FD_BSDRENO
#define NEED_SELECT_H
#define JMP_BUG sigjmp_buf
#define MIX_SOCKADDR

#elif defined(ULTRIX) || defined(__ULTRIX)
#define BSD
#define FD_BSD
#undef NO_KILLPG
#undef NO_SETSID
#define NEED_STRDUP
/* If you have Ultrix 4.3, and are using cc, const is broken */
#ifndef __ultrix__ /* Hack to check for pre-Ultrix 4.4 cc */
# define const /* Not implemented */
# define ULTRIX_BRAIN_DEATH
#endif /* __ultrix__ */
#define JMP_BUF sigjmp_buf
#define DIR_FILENO(p)  ((p)->dd_fd)
#define HEAD_GETPASS
#define HEAD_CRYPT

#elif defined(OSF1)
#ifndef BSD
/* # define BSD */
# include <sys/param.h> /* defines BSD */
#endif /* BSD */
#define FD_BSD
#undef NO_KILLPG
#undef NO_SETSID
#define JMP_BUF sigjmp_buf

#elif defined(SEQUENT)
#define BSD
#undef NO_KILLPG
#define NO_SETSID
#define NEED_STRDUP
#define tolower(c) (isupper(c) ? tolower(c) : c)

#elif defined(NeXT)
/* define BSD defined by default */
#define FD_BSD
#define NO_MALLOC_H
#undef NO_KILLPG
#define NO_SETSID
#define NEED_STRDUP
#undef _POSIX_SOURCE
#define NO_SIGLONGJMP
#define MIX_SOCKADDR
#if ! defined(_S_IFMT)
        #define _S_IFMT         0170000         /* type of file */
        #define    _S_IFDIR     0040000         /* directory */
        #define    _S_IFCHR     0020000         /* character special */
        #define    _S_IFBLK     0060000         /* block special */
        #define    _S_IFREG     0100000         /* regular */
        #define    _S_IFLNK     0120000         /* symbolic link */
        #define    _S_IFSOCK    0140000         /* socket */
        #define    _S_IFIFO     0010000         /* fifo (SUN_VFS) */
        #define _S_IRUSR        0000400         /* read permission, */
                                                /* owner */
        #define _S_IWUSR        0000200         /* write permission, */
                                                /* owner */
        #define _S_IXUSR        0000100         /* execute/search */
                                                /* permission, owner */

        #define S_ISGID         0002000         /* set group id on */
                                                /* execution */
        #define S_ISUID         0004000         /* set user id on */
                                                /* execution */
#endif /* ! defined(_S_IFMT) */

#if ! defined(S_IRUSR)
        #define S_IRUSR _S_IRUSR        /* read permission, owner */
        #define S_IRGRP 0000040         /* read permission, group */
        #define S_IROTH 0000004         /* read permission, other */
        #define S_IWUSR _S_IWUSR        /* write permission, owner */
        #define S_IWGRP 0000020         /* write permission, group */
        #define S_IWOTH 0000002         /* write permission, other */
        #define S_IXUSR _S_IXUSR        /* execute/search permission, */
                                        /* owner */
        #define S_IXGRP 0000010         /* execute/search permission, */
                                        /* group */
        #define S_IXOTH 0000001         /* execute/search permission, */
                                        /* other */
        #define S_IRWXU 0000700         /* read, write, execute */
                                        /* permissions, owner */
        #define S_IRWXG 0000070         /* read, write, execute */
                                        /* permissions, group */
        #define S_IRWXO 0000007         /* read, write, execute */
        #define S_IWUSR _S_IWUSR        /* write permission, owner */
        #define S_IWGRP 0000020         /* write permission, group */
        #define S_IWOTH 0000002         /* write permission, other */
        #define S_IXUSR _S_IXUSR        /* execute/search permission, */
                                        /* owner */
        #define S_IXGRP 0000010         /* execute/search permission, */
                                        /* group */
        #define S_IXOTH 0000001         /* execute/search permission, */
                                        /* other */
        #define S_IRWXU 0000700         /* read, write, execute */
                                        /* permissions, owner */
        #define S_IRWXG 0000070         /* read, write, execute */
                                        /* permissions, group */
        #define S_IRWXO 0000007         /* read, write, execute */
                                        /* permissions, other */

        #define S_ISBLK(mode)   (((mode) & (_S_IFMT)) == (_S_IFBLK))
        #define S_ISCHR(mode)   (((mode) & (_S_IFMT)) == (_S_IFCHR))
        #define S_ISDIR(mode)   (((mode) & (_S_IFMT)) == (_S_IFDIR))
        #define S_ISFIFO(mode)  (((mode) & (_S_IFMT)) == (_S_IFIFO))
        #define S_ISREG(mode)   (((mode) & (_S_IFMT)) == (_S_IFREG))
	#define S_ISLNK(mode)	(((mode) & (_S_IFMT)) == (_S_IFLNK))
#endif /* ! defined(S_IRUSR) */
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#define FD_CLOEXEC 01
#define waitpid(a,b,c) wait4(a,b,c,NULL)
#define getcwd(d,MAX_STRING_LEN) getwd(d);
typedef int pid_t;
typedef int mode_t;
#define JMP_BUF jmp_buf
#define DIR_FILENO(p)  ((p)->dd_fd)
/* The following might be required for some versions of NeXTStep on
 * some platforms.
 */
/* #include <netinet/in_systm.h> */


#elif defined(LINUX)
#if !defined(FD_LINUX) && !defined(NO_PASS)
#define FD_BSD
#define FD_BSDRENO
#endif
/* Needed for newer versions of libc (5.2.x) to use FD_LINUX hack */
#define DIRENT_ILLEGAL_ACCESS
#define DIR_FILENO(p)  ((p)->dd_fd)
#define CMSG_DATA(cmptr)  ((cmptr)->cmsg_data)
#define NEED_SYS_UN_H
#undef BSD
#undef NO_KILLPG
#undef NO_SETSID
#undef NEED_STRDUP
#define MIX_SOCKADDR
/* This are defined, in linux/time.h included from sys/time.h, as of 1.2.8 */
#ifdef NEVER_DEFINED
# define FD_SET __FD_SET
# define FD_ZERO __FD_ZERO
# define FD_ISSET __FD_ISSET
#endif /* NEVER_DEFINED */
#define JMP_BUF sigjmp_buf

#elif defined(NETBSD) || defined(__NetBSD__)
#define FD_BSD
#define FD_BSDRENO
#undef NO_KILLPG
#undef NO_SETSID
#define HAVE_SETPROCTITLE
#include <sys/types.h>

#elif defined(FreeBSD)
#define FD_BSD
#define FD_BSDRENO
#undef NO_KILLPG
#undef NO_SETSID
#define NO_MALLOC_H
#include <sys/types.h>

#elif defined(SCO)
#undef BSD
#undef NO_KILLPG
#undef NO_SETSID
#define NEED_INITGROUPS

#elif defined(SCO3)
#undef BSD
#define FD_SYSV
#define NEED_SPIPE
#undef NO_KILLPG
#undef NO_SETSID
#define NEED_INITGROUPS
#define CALL_TZSET
#define JMP_BUF sigjmp_buf
#define MIX_SOCKADDR

#elif defined(SCO5)
#undef BSD
#define FD_SYSV
#undef NO_KILLPG
#undef NO_SETSID
#define CALL_TZSET
#define JMP_BUF sigjmp_buf
#define MIX_SOCKADDR

#elif defined(CONVEXOS)
#define BSD
#define FD_BSD
#define NEED_STRDUP
#define NEED_SYS_MALLOC_H
#include <sys/types.h>
#define JMP_BUF sigjmp_buf

#elif defined(AUX)
#define BSD
#define FD_BSD
#undef NO_KILLPG
#undef NO_SETSID
#define NEED_STRDUP
#ifdef _POSIX_SOURCE
# define JMP_BUF sigjmp_buf
# undef __mc68000__
#else
# ifndef __mc68000__
#  define __mc68000__
# endif /* __mc68000__ */
# define JMP_BUF jmp_buf
# define STDIN_FILENO  0
# define STDOUT_FILENO 1
# define STDERR_FILENO 1
#endif /* _POSIX_SOURCE */


#elif defined(SVR4)
#define FD_BSD
#define NO_KILLPG
#undef  NO_SETSID
#undef NEED_STRDUP
#define NEED_STRCASECMP
#define NEED_STRNCASECMP
#define bzero(a,b) memset(a,0,b)
#define JMP_BUF sigjmp_buf
#define S_ISLNK(m) (((m)&(S_IFMT)) == (S_IFLNK))

#elif defined(__bsdi__)
#undef BSD
#define FD_BSD
#define FD_BSDRENO
#undef NO_KILLPG
#undef NO_SETSID
#define NO_GETDOMAINNAME
#define MIX_SOCKADDR
#define JMP_BUF sigjmp_buf
#define NEED_SYS_MALLOC_H

#elif defined(UTS21)
#undef BSD
#undef NO_KILLPG
#define NO_SETSID
#define NEED_WAITPID
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#define strftime(buf,bufsize,fmt,tm)    ascftime(buf,fmt,tm)
#include <sys/types.h>

#elif defined(APOLLO)
#define BSD
#undef NO_KILLPG
#undef NO_SETSID
#define NO_MALLOC_H
#define timezone	_bky_timezone
#include <sys/types.h>
#define DIR_FILENO(p)  ((p)->dd_fd)

#elif defined(ATTSVR3)
#define NO_STDLIB_H
#define NO_SYS_WAIT_H
#define NO_SETSID
#undef BSD
#undef NO_KILLPG
#define NO_STRFTIME
#undef NEED_STRDUP
#undef NEED_STRCASECMP
#undef NEED_STRNCASECMP
#define SIGCHLD SIGCLD
#define STDIN_FILENO fileno(stdin)
#define STDOUT_FILENO fileno(stdout)
#define STDERR_FILENO fileno(stderr)
#ifndef S_ISDIR
#define S_ISDIR(m)    (((m)&(S_IFMT)) == (S_IFDIR))
#endif
#ifndef S_ISREG
#define S_ISREG(m)    (((m)&(S_IFMT)) == (S_IFREG))
#endif
#define lstat stat
#define strftime(buf,bufsize,fmt,tm)    ascftime(buf,fmt,tm)
#define readlink(a,b,c) -1
typedef int uid_t;
typedef int gid_t;
typedef int pid_t;
extern struct group *getgrnam();
extern char *getenv();

#elif defined(__QNX__)
#define _POSIX_SOURCE
#define NEED_SELECT_H
#define NEED_INITGROUPS
#define wait3(a,b,c) waitpid(-1,a,b)

/* Unknown system - Edit these to match */
#else
/* BSD is whether your system uses BSD calls or System V calls. */
#define BSD
/* NO_KILLPG is set on systems that don't have killpg */
#undef NO_KILLPG
/* NO_SETSID is set on systems that don't have setsid */
#undef NO_SETSID
/* NEED_STRDUP is set on stupid systems that don't have strdup. */
#undef NEED_STRDUP
/* NO_PASS is set on systems that don't allow file descriptor passing */
#undef NO_PASS
/* FD_BSD is set on systems which pass file descriptors in a BSD way */
#undef FD_BSD
/* BSDRENO is for BSD 4.3RENO systems for file descriptor passing */
#undef BSDRENO
/* FD_SYSV is set on systems which pass file descriptor in a SYSV way */
#undef FD_SYSV
#endif /* System Types */

#if defined(__mc68000__)
# define NO_SIGLONGJMP
#endif /* defined(__mc68000__) */

/* If we haven't set anything about file descriptor passing, set NO_PASS */
#if !defined(FD_BSD) && !defined(FD_SYSV) && !defined(FD_LINUX) && !defined(NO_PASS) 
# define NO_PASS
#endif /* !defined(FD_BSD) && !defined(FD_SYSV) && !defined(NO_PASS) */

/*
 * The particular directory style your system supports. If you have dirent.h
 * in /usr/include (POSIX) or /usr/include/sys (SYSV), #include 
 * that file and define DIR_TYPE to be dirent. Otherwise, if you have 
 * /usr/include/sys/dir.h, define DIR_TYPE to be direct and include that
 * file. If you have neither, I'm confused.
 */

#if !defined(NeXT) && !defined(CONVEXOS) && !defined(APOLLO)
# include <dirent.h>
# define DIR_TYPE dirent
# ifndef DIR_FILENO
#  define DIR_FILENO(p) ((p)->dd_fd)
# endif /* DIR_FILENO */
#else
# include <sys/dir.h>
# define DIR_TYPE direct
#endif /* !defined(NeXT) && !defined(CONVEXOS) && !defined(APOLLO) */

#if !defined(HAVE_STDARG) && !defined(HAVE_VARARGS)
# define HAVE_STDARG
#endif /* !defined(HAVE_STDARG) && !defined(HAVE_VARARGS) */

#ifndef JMP_BUF
# define JMP_BUF sigjmp_buf
#endif /* JMP_BUF */
        
#ifndef NeXT
# include <unistd.h>
#endif /* NeXT */

#ifndef MAXPATHLEN
# include <sys/param.h>
#endif /* MAX_PATHLEN */

/* Some systems prefer sockaddr_in for some functions, and sock_addr
   for others 
 */
typedef struct sockaddr_in SERVER_SOCK_ADDR;
#ifdef MIX_SOCKADDR
typedef struct sockaddr CLIENT_SOCK_ADDR;
#else
typedef struct sockaddr_in CLIENT_SOCK_ADDR;
#endif /* MIX_SOCKADDR */

#ifdef AIX_BROKEN_HEADERS
/* string.h */
int strcasecmp(const char *, const char *);
int strncasecmp(const char *, const char *, size_t);
#include <sys/socket.h>
int accept(int, struct sockaddr *, int *);
int bind(int, struct sockaddr *, int);
int connect(int, struct sockaddr *, int);
int getpeername(int, struct sockaddr *, int *);
int getsockname(int, struct sockaddr *, int *);
int getsockopt(int, int, int, char *, int *);
int listen(int, int);
int recv(int, char *, int, int);
int recvfrom(int, char *, int, int, struct sockaddr *, int *);
int send(int, const char *, int, int);
int sendto(int, const char *, int, int, struct sockaddr *, int);
int setsockopt(int, int, int, const char *, int);
int socket(int, int, int);
int recvmsg(int, struct msghdr *, int);
int sendmsg(int, struct msghdr *, int);
int shutdown(int, int);
int socketpair(int, int, int, int *);
int killpg(int ProcessGroup, int Signal);
int initgroups(char* User, int BaseGID);
void bzero(char* String, int Length);
int gethostname(char* Name, int NameLength);
char *crypt(char* PW, char *Salt);
#endif /* AIX_BROKEN_HEADERS */


#endif /* _PORTABILITY_H_ */
