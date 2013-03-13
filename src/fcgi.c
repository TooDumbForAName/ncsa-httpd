/************************************************************************
 * FCGI Interface for the NCSA HTTPd Server
 *
 *  Copyright (C) 1995 Open Market, Inc.
 *  All rights reserved.
 *
 ************************************************************************
 * $Id: fcgi.c,v 1.3 1996/03/25 22:21:10 blong Exp $
 ************************************************************************
 *
 * fcgi.c -- interface to FCGI
 *
 *  Trung Dung
 *  tdung@openmarket.com
 *
 *  02-29-96 blong
 *    modified how files are included to the standard for other server
 *       source files for portability
 *    Shouldn't core dump is there isn't a configuration directive for
 *       the called fcgi.
 *    Don't send MIME-Version header (should probably look into using
 *      send_http_header, like everywhere else does)
 *    Need to turn off keepalive.  Should probably conditionally set
 *      it back on if a content-length header is returned, but...
 *
 *  03-25-96 blong
 *    Updating to newer version of FCGI spec as provided by Trung Dung of
 *      Openmarket.
 *
 *  06-25-96 blong
 *    Change select to infinite
 */

#include "config.h"
#include "portability.h"
  
#ifdef FCGI_SUPPORT
 

#include <stdio.h>
#ifndef NO_STDLIB_H
# include <stdlib.h>
#endif /* NO_STDLIB_H */
#ifndef NO_MALLOC_H
# ifdef NEED_SYS_MALLOC_H
#  include <sys/malloc.h>
# else
#  include <malloc.h>
# endif /* NEED_SYS_MALLOC_H */
#endif /* NO_MALLOC_H */
#include <fcntl.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <netdb.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <signal.h>
#ifdef NEED_SELECT_H
# include <sys/select.h>
#endif /* NEED_SELECT_H */
#include <sys/wait.h>
#include <pwd.h>
#include <grp.h>
/*--------------------------------------------------*/
#include "constants.h"
#include "fcgi.h"
#include "fdwrap.h"
#include "cgi.h"
#include "env.h"
#include "http_request.h"
#include "http_log.h"
#include "http_access.h"
#include "http_mime.h"
#include "http_config.h"
#include "http_auth.h"
#include "http_alias.h"
#include "util.h"
/*-----------------dependent types-----------------------*/
typedef per_request WS_Request;
#define WS_TimeOut(x)  send_fd_timed_out(x)
#define SERVER_ERROR SC_SERVER_ERROR
#define HostInfo(x) x->hostInfo
#define HostName(x) x->hostInfo->server_hostname
#define HostPort(x) port
#define OK 1
/*------------------------------------------------------------*/
#define FCGI_MAGIC_TYPE "application/x-httpd-fcgi"
#define DEFAULT_FCGI_LISTEN_Q 5
#define FCGI_DEFAULT_RESTART_DELAY 5
#define FCGI_MAX_PROCESSES 20
#define TRUE 1
#define FALSE 0
#define Malloc malloc
#define ASSERT assert
#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))

/*
 *  Tcl has a nice dynamic string library, but we want to insulate ourselves
 *  from the library names (we might not always be linked with Tcl, and we
 *  may want to implement our own dynamic string library in the future.)
 */
/*
 * The structure defined below is used to hold dynamic strings.  The only
 * field that clients should use is the string field, and they should
 * never modify it.
 */

#define TCL_DSTRING_STATIC_SIZE 200
typedef struct Tcl_DString {
    char *string;		/* Points to beginning of string:  either
				 * staticSpace below or a malloc'ed array. */
    int length;			/* Number of non-NULL characters in the
				 * string. */
    int spaceAvl;		/* Total number of bytes available for the
				 * string and its terminating NULL char. */
    char staticSpace[TCL_DSTRING_STATIC_SIZE];
				/* Space to use in common case where string
				 * is small. */
} Tcl_DString;

#define Tcl_DStringLength(dsPtr) ((dsPtr)->length)
#define Tcl_DStringValue(dsPtr) ((dsPtr)->string)
#define Tcl_DStringTrunc Tcl_DStringSetLength

#define DString			Tcl_DString
#define DStringAppend		Tcl_DStringAppend
#define DStringTrunc		Tcl_DStringSetLength
#define DStringValue		Tcl_DStringValue
#define DStringFree		Tcl_DStringFree
#define DStringLength		Tcl_DStringLength
#define DStringInit		Tcl_DStringInit
#define DStringAppendElement	Tcl_DStringAppendElement
#define DStringStartSublist	Tcl_DStringStartSublist
#define	DStringEndSublist	Tcl_DStringEndSublist
/* 
 * Macros to extract most-significant and least-significant bytes from
 * a 16-bit value. 
 */
#define MSB(x) ((x)/256)
#define LSB(x) ((x)%256)

/*------------------------------------------------------------*/

#define FCGI_ERROR -1

/*
 * Listening socket file number
 */
#define FCGI_LISTENSOCK_FILENO 0

/*
 * Value for version component of FCGI_Header
 */
#define FCGI_VERSION_1           1

/*
 * Fast CGI protocol version.
 */
#define FCGI_VERSION   FCGI_VERSION_1
/*
 * Values for type component of FCGI_Header
 */
#define FCGI_BEGIN_REQUEST       1
#define FCGI_ABORT_REQUEST       2
#define FCGI_END_REQUEST         3
#define FCGI_PARAMS              4
#define FCGI_STDIN               5
#define FCGI_STDOUT              6
#define FCGI_STDERR              7
#define FCGI_DATA                8
#define FCGI_GET_VALUES          9
#define FCGI_GET_VALUES_RESULT  10
#define FCGI_UNKNOWN_TYPE       11
#define FCGI_MAXTYPE (FCGI_UNKNOWN_TYPE)

/*
 * The length of the Fast CGI packet header.
 */
#define FCGI_HEADER_LEN	8

#define FCGI_MAX_LENGTH 0xffff


/*
 * This structure defines the layout of FastCGI packet headers.  ANSI C
 * compilers will guarantee the linear layout of this structure.
 */
typedef struct {
    unsigned char version;
    unsigned char type;
    unsigned char requestIdB1;
    unsigned char requestIdB0;
    unsigned char contentLengthB1;
    unsigned char contentLengthB0;
    unsigned char paddingLength;
    unsigned char reserved;
} FCGI_Header;


/*
 * Value for requestId component of FCGI_Header
 */
#define FCGI_NULL_REQUEST_ID     0


typedef struct {
    unsigned char roleB1;
    unsigned char roleB0;
    unsigned char flags;
    unsigned char reserved[5];
} FCGI_BeginRequestBody;

typedef struct {
    FCGI_Header header;
    FCGI_BeginRequestBody body;
} FCGI_BeginRequestRecord;

/*
 * Mask for flags component of FCGI_BeginRequestBody
 */
#define FCGI_KEEP_CONN  1

/*
 * Values for role component of FCGI_BeginRequestBody
 */
#define FCGI_RESPONDER  1
#define FCGI_AUTHORIZER 2
#define FCGI_FILTER     3


typedef struct {
    unsigned char appStatusB3;
    unsigned char appStatusB2;
    unsigned char appStatusB1;
    unsigned char appStatusB0;
    unsigned char protocolStatus;
    unsigned char reserved[3];
} FCGI_EndRequestBody;

typedef struct {
    FCGI_Header header;
    FCGI_EndRequestBody body;
} FCGI_EndRequestRecord;

/*
 * Values for protocolStatus component of FCGI_EndRequestBody
 */
#define FCGI_REQUEST_COMPLETE 0
#define FCGI_CANT_MPX_CONN    1
#define FCGI_OVERLOADED       2
#define FCGI_UNKNOWN_ROLE     3


/*
 * Variable names for FCGI_GET_VALUES / FCGI_GET_VALUES_RESULT records
 */
#define FCGI_MAX_CONNS  "FCGI_MAX_CONNS"
#define FCGI_MAX_REQS   "FCGI_MAX_REQS"
#define FCGI_MPXS_CONNS "FCGI_MPXS_CONNS"


typedef struct {
    unsigned char type;    
    unsigned char reserved[7];
} FCGI_UnknownTypeBody;

typedef struct {
    FCGI_Header header;
    FCGI_UnknownTypeBody body;
} FCGI_UnknownTypeRecord;



/*---------------------------------------------------------------*/
/*
 * This structure implements ring buffers, used to buffer data between
 * various processes and connections in the server.
 */
typedef struct Buffer {
    int size;               /* size of entire buffer */
    int length;		    /* number of bytes in current buffer */
    char *begin;            /* begining of valid data */
    char *end;              /* end of valid data */
    char data[1];           /* buffer data */
} Buffer;

/*
 * Size of the ring buffers used to read/write the FastCGI application server.
 */
#define SERVER_BUFSIZE	    8192
#define MAX_WRITE 4000
#define BufferLength(b)	    ((b)->length)
#define BufferFree(b)	    ((b)->size - (b)->length)
#define BufferSize(b)	    ((b)->size)

typedef void *OS_IpcAddress;

/*
 * OS Independent IPC Address Structure.
 */
typedef struct _OS_IpcAddr {
    DString bindPath;              /* Path used for the socket bind point */
    struct sockaddr *serverAddr;   /* server address (for connect) */
    int addrLen;		   /* length of server address (for connect) */
} OS_IpcAddr;

/*
 * The variable bindPathExtInt is used to create a Unix domain bindPath
 * for server managed processes.  This becomes part of the bindPath name.
 * See the function "OS_CreateLocalIpcFd" for it's use.
 */
static int bindPathExtInt = 1;

/*
 * The FCGI_LISTENSOCK_FILENO is the FD that the fast CGI process gets
 * handed when it starts up.  This is the FD that it listens to for
 * incoming connections from the Web Server.
 */
#define FCGI_LISTENSOCK_FILENO 0

typedef struct _FcgiProcessInfo {
    pid_t pid;                       /* pid of associated process */
    int listenFd;                    /* Listener socket */
    int fcgiFd;                      /* fcgi IPC file descriptor for
				      * persistent connections.
				      */
    OS_IpcAddress ipcAddr;           /* IPC Address of FCGI app server */
    struct _FastCgiServerInfo *serverInfoPtr;   /* Pointer to class parent */
} FcgiProcessInfo;


/*
 * This structure holds info for each Fast CGI server that's configured 
 * with this Web server.
 */
typedef struct _FastCgiServerInfo {
    DString execPath;               /* pathname of executable */
    char      **envp;               /* if NOT NULL, this is the env to send
				     * to the fcgi app when starting a server
				     * managed app.
				     */
    int listenQueueDepth;           /* size of listen queue for IPC */
    int maxProcesses;               /* max allowed processes of this class */
    int restartDelay;               /* number of seconds to wait between
				     * restarts after failure.  Can be zero.
				     */
    int restartOnExit;              /* = TRUE = restart. else terminate/free */
    int numRestarts;                /* Total number of restarts */
    int numFailures;                /* nun restarts due to exit failure */
    OS_IpcAddress ipcAddr;             /* IPC Address of FCGI app server.  This
				     * is the address used for all processes
				     */
    int listenFd;                   /* Listener socket */
    int processPriority;             /* if locally server managed process,
				      * this is the priority to run the
				      * processes in this class at.
				      */
    struct _FcgiProcessInfo *procInfo; /* This is a pointer to a list of
				     * processes related to this class.
				     */
    int reqRefCount;                 /* number of requests active for this
				      * server class.  The refCount also
				      * includes processes and connections
				      * to fcgi servers.
				      */
    int freeOnZero;                  /* Free structure when refCount = 0 */
    time_t procStartTime;            /* Process startup time */
    int restartTimerQueued;          /* = TRUE = restart timer queued */

    int keepConnection;              /* = 1 = maintain connection to app */
    int fcgiFd;                      /* fcgi IPC file descriptor for
				      * persistent connections. */
    pid_t procManager;
    struct _FastCgiServerInfo *next;
} FastCgiServerInfo;

/*
 * This structure holds the Fast CGI information for a particular request.
 */
typedef struct {
    int fd;			    /* connection to Fast CGI server */
    int gotHeader;		    /* TRUE if reading data bytes */
    unsigned char packetType;	    /* type of packet */
    int dataLen;		    /* length of data bytes */
    int paddingLen;                 /* record padding after content */
    FastCgiServerInfo *serverPtr;   /* Fast CGI server info */
    Buffer *inbufPtr;		    /* input buffer from server */
    Buffer *outbufPtr;		    /* output buffer to server */
    Buffer *reqInbufPtr;            /* client input buffer */
    Buffer *reqOutbufPtr;           /* client output buffer */
    DString *header;
    DString *errorOut;
    int parseHeader;
    WS_Request *reqPtr;
    int readingEndRequestBody;
    FCGI_EndRequestBody endRequestBody;
    Buffer *erBufPtr;
    int exitStatus;
    int exitStatusSet;
    int requestId;
} FastCgiInfo;

int fastCgiInit = 0;
static WS_Request *hackRequest = NULL;
FastCgiServerInfo *fastCgiServers = NULL;
FastCgiInfo *globalInfoPtr = NULL;
int ht_openmax = 128;
FILE *errorLogFd = NULL;
void PrepareClientSocket(WS_Request *reqPtr, int *csdIn, int *csdOut);
void MakeExtraEnvStr(WS_Request *reqPtr);
int SpawnChild(void (*func)(void *), void *data);
void SetErrorLogFd(void *inp, int type);
int WS_Access(const char *path, int mode);

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DStringInit --
 *
 *	Initializes a dynamic string, discarding any previous contents
 *	of the string (Tcl_DStringFree should have been called already
 *	if the dynamic string was previously in use).
 * Input: dsptr 
 *              Pointer to structure for dynamic string. 
 * 
 * Results:
 *	None.
 *
 * Side effects:
 *	The dynamic string is initialized to be empty.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_DStringInit(Tcl_DString *dsPtr)

{
    dsPtr->string = dsPtr->staticSpace;
    dsPtr->length = 0;
    dsPtr->spaceAvl = TCL_DSTRING_STATIC_SIZE;
    dsPtr->staticSpace[0] = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DStringAppend --
 *
 *	Append more characters to the current value of a dynamic string.
 * Input
 *   Tcl_DString *dsPtr;	Structure describing dynamic
 *				string.
 *   char *string;		 String to append.  If length is
 *				 -1 then this must be
 *				 null-terminated.
 *   int length;		 Number of characters from string
 *				to append.  If < 0, then append all
 *				 of string, up to null at end.
 *
 *
 * Results:
 *	The return value is a pointer to the dynamic string's new value.
 *
 * Side effects:
 *	Length bytes from string (or all of string if length is less
 *	than zero) are added to the current value of the string.  Memory
 *	gets reallocated if needed to accomodate the string's new size.
 *
 *----------------------------------------------------------------------
 */

char *
Tcl_DStringAppend(Tcl_DString *dsPtr, char *string, int length)
{
    int newSize;
    char *newString, *dst, *end;

    if (length < 0) {
	length = strlen(string);
    }
    newSize = length + dsPtr->length;

    /*
     * Allocate a larger buffer for the string if the current one isn't
     * large enough.  Allocate extra space in the new buffer so that there
     * will be room to grow before we have to allocate again.
     */

    if (newSize >= dsPtr->spaceAvl) {
	dsPtr->spaceAvl = newSize*2;
	newString = (char *) Malloc((unsigned) dsPtr->spaceAvl);
	memcpy((void *)newString, (void *) dsPtr->string,
		(size_t) dsPtr->length);
	if (dsPtr->string != dsPtr->staticSpace) {
	    free(dsPtr->string);
	}
	dsPtr->string = newString;
    }

    /*
     * Copy the new string into the buffer at the end of the old
     * one.
     */

    for (dst = dsPtr->string + dsPtr->length, end = string+length;
	    string < end; string++, dst++) {
	*dst = *string;
    }
    *dst = 0;
    dsPtr->length += length;
    return dsPtr->string;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DStringSetLength --
 *
 *	Change the length of a dynamic string.  This can cause the
 *	string to either grow or shrink, depending on the value of
 *	length.
 * Input:
 *      Tcl_DString *dsPtr;	Structure describing dynamic string
 *      int length;		New length for dynamic string.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The length of dsPtr is changed to length and a null byte is
 *	stored at that position in the string.  If length is larger
 *	than the space allocated for dsPtr, then a panic occurs.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_DStringSetLength(Tcl_DString *dsPtr, int length)
{
    if (length < 0) {
	length = 0;
    }
    if (length >= dsPtr->spaceAvl) {
	char *newString;

	dsPtr->spaceAvl = length+1;
	newString = (char *) Malloc((unsigned) dsPtr->spaceAvl);

	/*
	 * SPECIAL NOTE: must use memcpy, not strcpy, to copy the string
	 * to a larger buffer, since there may be embedded NULLs in the
	 * string in some cases.
	 */

	memcpy((void *) newString, (void *) dsPtr->string,
		(size_t) dsPtr->length);
	if (dsPtr->string != dsPtr->staticSpace) {
	    free(dsPtr->string);
	}
	dsPtr->string = newString;
    }
    dsPtr->length = length;
    dsPtr->string[length] = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_StringMatch --
 *
 *	See if a particular string matches a particular pattern.
 *
 * Results:
 *	The return value is 1 if string matches pattern, and
 *	0 otherwise.  The matching operation permits the following
 *	special characters in the pattern: *?\[] (see the manual
 *	entry for details on what these mean).
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_StringMatch(string, pattern)
    register char *string;	/* String. */
    register char *pattern;	/* Pattern, which may contain
				 * special characters. */
{
    char c2;

    while (1) {
	/* See if we're at the end of both the pattern and the string.
	 * If so, we succeeded.  If we're at the end of the pattern
	 * but not at the end of the string, we failed.
	 */
	
	if (*pattern == 0) {
	    if (*string == 0) {
		return 1;
	    } else {
		return 0;
	    }
	}
	if ((*string == 0) && (*pattern != '*')) {
	    return 0;
	}

	/* Check for a "*" as the next pattern character.  It matches
	 * any substring.  We handle this by calling ourselves
	 * recursively for each postfix of string, until either we
	 * match or we reach the end of the string.
	 */
	
	if (*pattern == '*') {
	    pattern += 1;
	    if (*pattern == 0) {
		return 1;
	    }
	    while (1) {
		if (Tcl_StringMatch(string, pattern)) {
		    return 1;
		}
		if (*string == 0) {
		    return 0;
		}
		string += 1;
	    }
	}
    
	/* Check for a "?" as the next pattern character.  It matches
	 * any single character.
	 */

	if (*pattern == '?') {
	    goto thisCharOK;
	}

	/* Check for a "[" as the next pattern character.  It is followed
	 * by a list of characters that are acceptable, or by a range
	 * (two characters separated by "-").
	 */
	
	if (*pattern == '[') {
	    pattern += 1;
	    while (1) {
		if ((*pattern == ']') || (*pattern == 0)) {
		    return 0;
		}
		if (*pattern == *string) {
		    break;
		}
		if (pattern[1] == '-') {
		    c2 = pattern[2];
		    if (c2 == 0) {
			return 0;
		    }
		    if ((*pattern <= *string) && (c2 >= *string)) {
			break;
		    }
		    if ((*pattern >= *string) && (c2 <= *string)) {
			break;
		    }
		    pattern += 2;
		}
		pattern += 1;
	    }
	    while (*pattern != ']') {
		if (*pattern == 0) {
		    pattern--;
		    break;
		}
		pattern += 1;
	    }
	    goto thisCharOK;
	}
    
	/* If the next pattern character is '/', just strip off the '/'
	 * so we do exact matching on the character that follows.
	 */
	
	if (*pattern == '\\') {
	    pattern += 1;
	    if (*pattern == 0) {
		return 0;
	    }
	}

	/* There's no special character.  Just make sure that the next
	 * characters of each string match.
	 */
	
	if (*pattern != *string) {
	    return 0;
	}

	thisCharOK: pattern += 1;
	string += 1;
    }
}


/*
 *----------------------------------------------------------------------
 *
 * Tcl_DStringFree --
 *
 *	Frees up any memory allocated for the dynamic string and
 *	reinitializes the string to an empty state.
 * Input:
 *     Tcl_DString *dsPtr;	Structure describing dynamic string
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The previous contents of the dynamic string are lost, and
 *	the new value is an empty string.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_DStringFree(Tcl_DString *dsPtr)
{
    if (dsPtr->string != dsPtr->staticSpace) {
	free(dsPtr->string);
    }
    dsPtr->string = dsPtr->staticSpace;
    dsPtr->length = 0;
    dsPtr->spaceAvl = TCL_DSTRING_STATIC_SIZE;
    dsPtr->staticSpace[0] = 0;
}
/*------------------------------------------------------------*/

/*
 *----------------------------------------------------------------------
 *
 * OS_InitIpcAddr --
 *
 *	Initialize the OS specific IPC address used for FCGI I/O.
 *
 * Results:
 *      IPC Address is initialized.
 *
 * Side effects:  
 *      None.
 *
 *----------------------------------------------------------------------
 */  
OS_IpcAddress OS_InitIpcAddr(void)
{
    OS_IpcAddr *ipcAddrPtr = (OS_IpcAddr *)Malloc(sizeof(OS_IpcAddr));
    DStringInit(&ipcAddrPtr->bindPath);
    ipcAddrPtr->serverAddr = NULL;
    ipcAddrPtr->addrLen = 0;
    return (OS_IpcAddress)ipcAddrPtr;
}

int OS_Bind(unsigned int sock, struct sockaddr *addr, int namelen)
{
    return(bind(sock, addr, namelen));
}

int OS_Listen(unsigned int sock, int backlog)
{
    return(listen(sock, backlog));
}

int OS_Socket(int addr_family, int type, int protocol)
{
    return (socket(addr_family, type, protocol));
}

int OS_Close(int fd)
{
    return close(fd);
}

int OS_Dup2(int oldd,int newd)
{
    int fd;

    fd = dup2(oldd, newd);
    return fd;
}


int OS_Fcntl(int fd, int cmd, int arg)
{
    return(fcntl(fd, cmd, arg));
}

int OS_Read(int fd, void *buf, size_t numBytes)
{
    int result;

    while (1) {
        result = read(fd, buf, (size_t) numBytes);
        if ((result != -1) || (errno != EINTR)) {
            return result;
        }
    }
}

int OS_Write(int fd, void *buf, size_t numBytes)
{
    int result;

    while (1) {
        result = write(fd, buf, (size_t) numBytes);
        if ((result != -1) || (errno != EINTR)) {
            return result;
        }
    }
}

/*
 *----------------------------------------------------------------------
 *
 * BufferCheck --
 *
 *	Checks buffer for consistency with a set of assertions.
 *
 *	If assert() is a no-op, this routine should be optimized away
 *	in most C compilers.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
void BufferCheck(Buffer *bufPtr)
{
    ASSERT(bufPtr->size > 0);
    ASSERT(bufPtr->length >= 0);
    ASSERT(bufPtr->length <= bufPtr->size);

    ASSERT(bufPtr->begin >= bufPtr->data);
    ASSERT(bufPtr->begin < bufPtr->data + bufPtr->size);
    ASSERT(bufPtr->end >= bufPtr->data);
    ASSERT(bufPtr->end < bufPtr->data + bufPtr->size);

    ASSERT(((bufPtr->end - bufPtr->begin + bufPtr->size) % bufPtr->size) 
	    == (bufPtr->length % bufPtr->size));
}

/*
 *----------------------------------------------------------------------
 *
 * BufferReset --
 *
 *	Reset a buffer, losing any data that's in it.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
void BufferReset(Buffer *bufPtr)
{
    bufPtr->length = 0;
    bufPtr->begin = bufPtr->end = bufPtr->data;
}

/*
 *----------------------------------------------------------------------
 *
 * BufferCreate --
 *
 *	Allocate an intialize a new buffer of the specified size.
 *
 * Results:
 *      Pointer to newly allocated buffer.
 *
 * Side effects:
 *      None.                     
 *
 *----------------------------------------------------------------------
 */
Buffer *BufferCreate(int size)
{
    Buffer *bufPtr;

    bufPtr = (Buffer *)Malloc(sizeof(Buffer) + size);
    bufPtr->size = size;
    BufferReset(bufPtr);
    return bufPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * BufferDelete --
 *
 *      Delete a buffer, freeing up any associated storage.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
void BufferDelete(Buffer *bufPtr)
{
    BufferCheck(bufPtr);
    free(bufPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * BufferRead --
 *
 *	Read bytes from an open file descriptor into a buffer.
 *
 * Results:
 *      Returns number of bytes read.
 *
 * Side effects:
 *      Data stored in buffer.
 *
 *----------------------------------------------------------------------
 */
int BufferRead(Buffer *bufPtr, int fd)
{
    int len;

    BufferCheck(bufPtr);
    len = min(bufPtr->size - bufPtr->length, 
	    bufPtr->data + bufPtr->size - bufPtr->end);

    if (len > 0) {
        signal(SIGPIPE,SIG_IGN);
        len = OS_Read(fd, bufPtr->end, len);
	if(len > 0) {
	    bufPtr->end += len;
	    if(bufPtr->end >= (bufPtr->data + bufPtr->size))
		bufPtr->end -= bufPtr->size;
	    bufPtr->length += len;
	}
    }
    return len;
}

void FcgiCleanUp(FastCgiInfo *infoPtr)
{
  char *p = DStringValue(infoPtr->errorOut);
  if (p)
    fprintf(errorLogFd, "%s", p);
  BufferDelete(infoPtr->inbufPtr);
  BufferDelete(infoPtr->outbufPtr);
  BufferDelete(infoPtr->reqInbufPtr);
  BufferDelete(infoPtr->reqOutbufPtr);
  BufferDelete(infoPtr->erBufPtr);
  DStringFree(infoPtr->header);
  DStringFree(infoPtr->errorOut);
  OS_Close(infoPtr->fd);
  free(infoPtr);
  fflush(errorLogFd);
}

void fcgi_timed_out(int sigcode)
{
  FcgiCleanUp(globalInfoPtr);
  WS_TimeOut(sigcode);
}

/*
 *----------------------------------------------------------------------
 *
 * BufferWrite --
 *
 *	Write any bytes from the buffer to a file descriptor open for
 *	writing.
 *
 * Results:
 *      Returns number of bytes written.
 *
 * Side effects:
 *      Data "removed" from buffer.
 *
 *----------------------------------------------------------------------
 */
int BufferWrite(Buffer *bufPtr, int fd)
{
    int len;

    ASSERT(fd >= 0);
    signal(SIGALRM,fcgi_timed_out);
    signal(SIGPIPE,fcgi_timed_out); 
    alarm(timeout);
    BufferCheck(bufPtr);
    len = min(bufPtr->length, bufPtr->data + bufPtr->size - bufPtr->begin);

    /* should the same fix be made in core server? */
    if (len > MAX_WRITE)
      len = MAX_WRITE;
    if(len > 0) {
	len = OS_Write(fd, bufPtr->begin, len);
	if(len > 0) {
	    bufPtr->begin += len;
	    if(bufPtr->begin >= (bufPtr->data + bufPtr->size))
		bufPtr->begin -= bufPtr->size;
	    bufPtr->length -= len;
	}
    }
    alarm(0);
    signal(SIGALRM,SIG_IGN);
    signal(SIGPIPE,SIG_IGN);
    return len;
}

/*
 *----------------------------------------------------------------------
 *
 * BufferToss --
 *
 *	Throw away the specified number of bytes from a buffer, as if
 *	they had been written out.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Data "removed" from the buffer.
 *
 *----------------------------------------------------------------------
 */
void BufferToss(Buffer *bufPtr, int count)
{
    BufferCheck(bufPtr);
    ASSERT(count >= 0 && count <= bufPtr->length);

    bufPtr->length -= count;
    bufPtr->begin += count;
    if(bufPtr->begin >= bufPtr->data + bufPtr->size)
	bufPtr->begin -= bufPtr->size;
}

/*
 *----------------------------------------------------------------------
 *
 * BufferExpand --
 *
 *	Expands the buffer by the specified number of bytes.  Assumes that
 *	the caller has added the data to the buffer.  This is typically
 *	used after a BufferAsyncRead() call completes, to update the buffer
 *	size with the number of bytes read.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Data "added" to the buffer.
 *
 *----------------------------------------------------------------------
 */
void BufferExpand(Buffer *bufPtr, int count)
{
    BufferCheck(bufPtr);
    ASSERT(count >= 0 && count <= BufferFree(bufPtr));

    bufPtr->length += count;
    bufPtr->end += count;
    if(bufPtr->end >= bufPtr->data + bufPtr->size)
	bufPtr->end -= bufPtr->size;

    BufferCheck(bufPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * BufferAddData --
 *
 *	Adds data to a buffer, returning the number of bytes added.
 *
 * Results:
 *      Number of bytes added to the buffer.
 *
 * Side effects:
 *      Characters added to the buffer.
 *
 *----------------------------------------------------------------------
 */
int BufferAddData(Buffer *bufPtr, char *data, int datalen)
{
    char *end;
    int copied = 0;	/* Number of bytes actually copied. */
    int canCopy;		/* Number of bytes to copy in a given op. */

    ASSERT(data != NULL);
    if(datalen == 0)
	return 0;

    ASSERT(datalen > 0);

    BufferCheck(bufPtr);
    end = bufPtr->data + bufPtr->size;

    /*
     * Copy the first part of the data:  from here to the end of the
     * buffer, or the end of the data, whichever comes first.
     */
    datalen = min(BufferFree(bufPtr), datalen);
    canCopy = min(datalen, end - bufPtr->end);
    memcpy(bufPtr->end, data, canCopy);
    bufPtr->length += canCopy;
    bufPtr->end += canCopy;
    copied += canCopy;
    if (bufPtr->end >= end)
	bufPtr->end = bufPtr->data;
    datalen -= canCopy;

    /*
     * If there's more to go, copy the second part starting from the
     * beginning of the buffer.
     */
    if (datalen > 0) {
	data += canCopy;
	memcpy(bufPtr->end, data, datalen);
	bufPtr->length += datalen;
	bufPtr->end += datalen;
	copied += datalen;
    }
    return(copied);
}

/*
 *----------------------------------------------------------------------
 *
 * BufferAdd --
 *
 *	Adds a string into a buffer, returning the number of bytes added.
 *
 * Results:
 *      Number of bytes added to the buffer.
 *
 * Side effects:
 *      Characters added to the buffer.
 *
 *----------------------------------------------------------------------
 */
int BufferAdd(Buffer *bufPtr, char *str)
{
    return BufferAddData(bufPtr, str, strlen(str));
}

/*
 *----------------------------------------------------------------------
 *
 * BufferGetc --
 *
 *      Gets a character from a buffer.  The buffer must be non-emtpy.
 *
 * Results:
 *	The character.
 *
 * Side effects:
 *      One character removed from the buffer.
 *
 *----------------------------------------------------------------------
 */
char BufferGetc(Buffer *bufPtr)
{
    char c;

    BufferCheck (bufPtr);
    ASSERT(BufferLength(bufPtr) != 0);
    c = *bufPtr->begin++;
    bufPtr->length--;
    if (bufPtr->begin == bufPtr->data + bufPtr->size) {
        bufPtr->begin = bufPtr->data;
    }
    return c;
}

/*
 *----------------------------------------------------------------------
 *
 * BufferCopyTillStr --
 *
 *      This function moves bytes from an input buffer to an output buffer
 *      until either an entire match is found in the input buffer, or
 *	the length of the input buffer is smaller than the length of
 *	the match string.
 *
 *	When the match string is found, it is "consumed" and not copied
 *	to the output buffer.  The input buffer is left at the next
 *	valid character after the match string.
 *
 *	The partially parsed state (stored as an index into the match
 *	string) is stored in *ppstate.
 *
 * Results:
 *	TRUE if complete match string found, FALSE otherwise.
 *
 * Side effects:
 *	Bytes moved from input to output buffer.
 *
 *----------------------------------------------------------------------
 */
int BufferCopyTillStr(Buffer * in_buf, Buffer * out_buf, char *match_str,
                          int *ppstate)
{
    int len;
    int match_str_len;
    char c;
    int bytes_in, bytes_out;    /* keep track of the buffer's space locally */
    int l_ppstate;              /* local partial parse state */

    BufferCheck(in_buf);
    BufferCheck(out_buf);

    match_str_len = strlen(match_str);

    l_ppstate = *ppstate;
    bytes_out = BufferFree(out_buf) - match_str_len + 1;
    bytes_in = BufferLength(in_buf);

    while (bytes_in > 0 && bytes_out > 0) {
        c = BufferGetc(in_buf);
        bytes_in--;
        if (c == match_str[l_ppstate]) {
            l_ppstate++;
            if (match_str[l_ppstate] == '\0') {
                *ppstate = 0;
                return TRUE;
            }
        } else {
            len = BufferAddData(out_buf, match_str, l_ppstate);
	    ASSERT(len == l_ppstate);
            bytes_out -= l_ppstate;
            if (c == match_str[0]) {
                l_ppstate = 1;
            } else {
                len = BufferAddData(out_buf, &c, 1);
		ASSERT(len == 1);
                bytes_out--;
                l_ppstate = 0;
            }
        }
    }
    *ppstate = l_ppstate;
    return FALSE;
}

/*
 *----------------------------------------------------------------------
 *
 * BufferGetData --
 *
 *	Gets data from a buffer, returning the number of bytes copied.
 *
 * Results:
 *      Number of bytes copied from the buffer.
 *
 * Side effects:
 *      Updates the buffer pointer.
 *
 *----------------------------------------------------------------------
 */
int BufferGetData(Buffer *bufPtr, char *data, int datalen)
{
    char *end;
    int copied = 0;		   /* Number of bytes actually copied. */
    int canCopy;		   /* Number of bytes to copy in a given op. */

    ASSERT(data != NULL);
    ASSERT(datalen > 0);
    BufferCheck(bufPtr);
    end = bufPtr->data + bufPtr->size;

    /*
     * Copy the first part out of the buffer: from here to the end
     * of the buffer, or all of the requested data.
     */
    canCopy = min(bufPtr->length, datalen);
    canCopy = min(canCopy, end - bufPtr->begin);
    memcpy(data, bufPtr->begin, canCopy);
    bufPtr->length -= canCopy;
    bufPtr->begin += canCopy;
    copied += canCopy;
    if (bufPtr->begin >= end)
	bufPtr->begin = bufPtr->data;

    /*
     * If there's more to go, copy the second part starting from the
     * beginning of the buffer.
     */
    if (copied < datalen && bufPtr->length > 0) {
	data += copied;
	canCopy = min(bufPtr->length, datalen - copied);
	memcpy(data, bufPtr->begin, canCopy);
	bufPtr->length -= canCopy;
	bufPtr->begin += canCopy;
	copied += canCopy;
    }
    BufferCheck(bufPtr);
    return(copied);
}

/*
 *----------------------------------------------------------------------
 *
 * BufferMove --
 *
 *	Move the specified number of bytes from one buffer to another.
 *	There must be at least 'len' bytes available in the source buffer,
 *	and space for 'len' bytes in the destination buffer.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Bytes moved.
 *
 *----------------------------------------------------------------------
 */
void BufferMove(Buffer *toPtr, Buffer *fromPtr, int len)
{
    int fromLen, toLen, toMove;
    
    ASSERT(len > 0);
    ASSERT(BufferLength(fromPtr) >= len);
    ASSERT(BufferFree(toPtr) >= len);

    BufferCheck(toPtr);
    BufferCheck(fromPtr);

    for(;;) {
	fromLen = min(fromPtr->length, 
		fromPtr->data + fromPtr->size - fromPtr->begin);

	toLen = min(toPtr->size - toPtr->length, 
		toPtr->data + toPtr->size - toPtr->end);

	toMove = min(fromLen, toLen);
	toMove = min(toMove, len);

	ASSERT(toMove >= 0);
	if(toMove == 0)
	    return;

	memcpy(toPtr->end, fromPtr->begin, toMove);
	BufferToss(fromPtr, toMove);
	BufferExpand(toPtr, toMove);
	len -= toMove;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * BufferDStringAppend --
 *
 *	Append the specified number of bytes from a buffer onto the 
 *	end of a DString.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Bytes moved.
 *
 *----------------------------------------------------------------------
 */
void BufferDStringAppend(DString *strPtr, Buffer *bufPtr, int len)
{
    int fromLen;

    BufferCheck(bufPtr);
    ASSERT(len > 0);
    ASSERT(len <= BufferLength(bufPtr));

    while(len > 0) {
        fromLen = min(len, bufPtr->data + bufPtr->size - bufPtr->begin);

	ASSERT(fromLen > 0);
	DStringAppend(strPtr, bufPtr->begin, fromLen);
	BufferToss(bufPtr, fromLen);
	len -= fromLen;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * OS_BuildSockAddrUn --
 *
 *      Using the pathname bindPath, fill in the sockaddr_un structure
 *      *servAddrPtr and the length of this structure *servAddrLen.
 *
 *      The format of the sockaddr_un structure changed incompatibly in
 *      4.3BSD Reno.
 *
 * Results:
 *      0 for normal return, -1 for failure (bindPath too long).
 *
 *----------------------------------------------------------------------
 */

static int OS_BuildSockAddrUn(char *bindPath,
                              struct sockaddr_un *servAddrPtr,
                              int *servAddrLen)
{
    int bindPathLen = strlen(bindPath);

#ifdef HAVE_SOCKADDR_UN_SUN_LEN /* 4.3BSD Reno and later: BSDI */
    if(bindPathLen >= sizeof(servAddrPtr->sun_path))
        return -1;
#else                           /* 4.3 BSD Tahoe: Solaris, HPUX, DEC, ... */
    if(bindPathLen > sizeof(servAddrPtr->sun_path))
        return -1;
#endif
    memset((char *) servAddrPtr, 0, sizeof(*servAddrPtr));
    servAddrPtr->sun_family = AF_UNIX;
    memcpy(servAddrPtr->sun_path, bindPath, bindPathLen);
#ifdef HAVE_SOCKADDR_UN_SUN_LEN /* 4.3BSD Reno and later: BSDI */
    *servAddrLen = sizeof(servAddrPtr->sun_len)
            + sizeof(servAddrPtr->sun_family)
            + bindPathLen + 1;
    servAddrPtr->sun_len = *servAddrLen;
#else                           /* 4.3 BSD Tahoe: Solaris, HPUX, DEC, ... */
    *servAddrLen = sizeof(servAddrPtr->sun_family) + bindPathLen;
#endif
    return 0;
}

int Die(FastCgiInfo *infoPtr, int err_code, char *msg)
{
  FcgiCleanUp(infoPtr);
  die(infoPtr->reqPtr, err_code, msg);
  return SERVER_ERROR;
}

/*
 *----------------------------------------------------------------------
 * 
 * ConnectionError --
 *
 *	This routine gets called when there's an error in connecting to
 *	a FastCGI server.  It returns an error message to the client
 *	and shuts down the request.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	An error message is returned, and the request is completed.
 * 
 *----------------------------------------------------------------------
 */
static void ConnectionError(FastCgiInfo *infoPtr, int errorCode)
{
  char error_msg[MAX_STRING_LEN];
  char *msg = strerror(errorCode);
  if (msg == NULL)
    msg = "";
  sprintf(error_msg, "HTTPd: error connecting to fastcgi server - filename: %s - error: %s ",
	  infoPtr->reqPtr->filename, msg);
  Die(infoPtr,SERVER_ERROR,error_msg);
}

/*------------------------------------------------------------*/

FastCgiServerInfo *FastCgiServerInfoLookup(char *suffix)
{
  FastCgiServerInfo *info;
  info = fastCgiServers;
  while (info) {
    const char *ePath = DStringValue(&info->execPath);
    if (ePath && (strcmp(ePath, suffix) == 0))
      return info;
    info = info->next;
  }
  return NULL;
}
/*------------------------------------------------------------*/
/*
 *----------------------------------------------------------------------
 * 
 * SendPacketHeader --
 *
 *	Assembles and sends the FastCGI packet header for a given 
 *	request.  It is the caller's responsibility to make sure that
 *	there's enough space in the buffer, and that the data bytes
 *	(specified by 'len') are queued immediately following this
 *	header.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Packet header queued.
 * 
 *----------------------------------------------------------------------
 */
static void SendPacketHeader(FastCgiInfo *infoPtr, int type, int len)
{
    FCGI_Header header;

    ASSERT(type > 0 && type <= FCGI_MAXTYPE);
    ASSERT(len >= 0);
    ASSERT(BufferFree(infoPtr->outbufPtr) > sizeof(FCGI_Header));

    /*
     * Assemble and queue the packet header.
     */
    header.version = FCGI_VERSION;
    header.type = type;
    header.requestIdB1 = (infoPtr->requestId >> 8) & 0xff;
    header.requestIdB0 = (infoPtr->requestId) & 0xff;
    header.contentLengthB1 = MSB(len);
    header.contentLengthB0 = LSB(len);
    header.paddingLength = 0;
    header.reserved = 0;
    BufferAddData(infoPtr->outbufPtr, (char *) &header, sizeof(FCGI_Header));
}

/*
 *----------------------------------------------------------------------
 *
 * MakeBeginRequestBody --
 *
 *      Constructs an FCGI_BeginRequestBody record.
 *
 *----------------------------------------------------------------------
 */
static void MakeBeginRequestBody(int role,
				 int keepConnection,
				 FCGI_BeginRequestBody *body)
{
    ASSERT((role >> 16) == 0);
    body->roleB1 = (role >>  8) & 0xff;
    body->roleB0 = (role      ) & 0xff;
    body->flags = (keepConnection) ? FCGI_KEEP_CONN : 0;
    memset(body->reserved, 0, sizeof(body->reserved));
}

/*
 *----------------------------------------------------------------------
 * 
 * SendBeginRequest - 
 *
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Begin request queued.
 * 
 *----------------------------------------------------------------------
 */
static void SendBeginRequest(FastCgiInfo *infoPtr)
{
  FCGI_BeginRequestBody body;
  unsigned int bodySize;

  /*
   * We should be the first ones to use this buffer.
   */
  ASSERT(BufferLength(infoPtr->outbufPtr) == 0);

  bodySize = sizeof(FCGI_BeginRequestBody);
  MakeBeginRequestBody(FCGI_RESPONDER, TRUE, &body);
  SendPacketHeader(infoPtr, FCGI_BEGIN_REQUEST, bodySize);
  BufferAddData(infoPtr->outbufPtr, (char *) &body, bodySize);
}

/*
 *----------------------------------------------------------------------
 *
 * FCGIUtil_BuildNameValueHeader --
 *
 *      Builds a name-value pair header from the name length
 *      and the value length.  Stores the header into *headerBuffPtr,
 *      and stores the length of the header into *headerLenPtr.
 *
 * Side effects:
 *      Stores header's length (at most 8) into *headerLenPtr,
 *      and stores the header itself into
 *      headerBuffPtr[0 .. *headerLenPtr - 1].
 *
 *----------------------------------------------------------------------
 */
static void FCGIUtil_BuildNameValueHeader(
        int nameLen,
        int valueLen,
        unsigned char *headerBuffPtr,
        int *headerLenPtr) {
    unsigned char *startHeaderBuffPtr = headerBuffPtr;

    ASSERT(nameLen >= 0);
    if(nameLen < 0x80) {
        *headerBuffPtr++ = nameLen;
    } else {
        *headerBuffPtr++ = (nameLen >> 24) | 0x80;
        *headerBuffPtr++ = (nameLen >> 16);
        *headerBuffPtr++ = (nameLen >> 8);
        *headerBuffPtr++ = nameLen;
    }
    ASSERT(valueLen >= 0);
    if(valueLen < 0x80) {
        *headerBuffPtr++ = valueLen;
    } else {
        *headerBuffPtr++ = (valueLen >> 24) | 0x80;
        *headerBuffPtr++ = (valueLen >> 16);
        *headerBuffPtr++ = (valueLen >> 8);
        *headerBuffPtr++ = valueLen;
    }
    *headerLenPtr = headerBuffPtr - startHeaderBuffPtr;
}

/*
 *----------------------------------------------------------------------
 * 
 * SendEnvironment --
 *
 *	Queue the environment variables to a FastCGI server.  Assumes that
 *	there's enough space in the output buffer to hold the variables.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Environment variables queued for delivery.
 * 
 *----------------------------------------------------------------------
 */
static void SendEnvironment(WS_Request *reqPtr, FastCgiInfo *infoPtr)
{
    int headerLen, nameLen, valueLen;
    char *equalPtr;
    unsigned char headerBuff[8];
    int content, i;


    /*
     * Send each environment item to the FastCGI server as a 
     * FastCGI format name-value pair.
     *
     * XXX: this code will break with the environment format used on NT.
     */

    MakeExtraEnvStr(reqPtr);
    add_common_vars(reqPtr);
    add_cgi_vars(reqPtr, &content);
    i = reqPtr->num_env - 1;
    while(i >= 0) {
        equalPtr = strchr(reqPtr->env[i], '=');
        ASSERT(equalPtr != NULL);
        nameLen = equalPtr - reqPtr->env[i];
        valueLen = strlen(equalPtr + 1);
        FCGIUtil_BuildNameValueHeader(
                nameLen,
                valueLen,
                &headerBuff[0],
                &headerLen);
	SendPacketHeader(
                infoPtr,
                FCGI_PARAMS,
                headerLen + nameLen + valueLen);
	BufferAddData(infoPtr->outbufPtr, (char *) &headerBuff[0], headerLen);
	BufferAddData(infoPtr->outbufPtr, reqPtr->env[i], nameLen);
	BufferAddData(infoPtr->outbufPtr, equalPtr + 1, valueLen);
	i--;
    }
    SendPacketHeader(infoPtr, FCGI_PARAMS, 0);
}


/*
 *----------------------------------------------------------------------
 *   
 * ClientToCgiBuffer --
 *
 *	Move bytes from the client to the FastCGI server as standard 
 *	input packets.
 *             
 * Results:
 *      None.
 *
 * Side effects:
 *      Bytes moved from client input to FastCGI server output.
 *
 *----------------------------------------------------------------------
 */
static void ClientToCgiBuffer(FastCgiInfo *infoPtr)
{
    int maxlen;
    int in_len, out_free;

    /*
     * If there's no input data, or there's not enough space in the output
     * buffer for any data bytes, then there's nothing we can do.
     */
    in_len = BufferLength(infoPtr->reqInbufPtr);
    out_free = BufferFree(infoPtr->outbufPtr) - sizeof(FCGI_Header);
    maxlen = min (in_len, out_free);

    if(maxlen > 0) {
        SendPacketHeader(infoPtr, FCGI_STDIN, maxlen);
	BufferMove(infoPtr->outbufPtr, infoPtr->reqInbufPtr, maxlen);
    }
}

/*
 *----------------------------------------------------------------------
 *   
 * CgiToClientBuffer --
 *
 *	Process packets from FastCGI server.
 *             
 * Results:
 *      None.
 *
 * Side effects:
 *      Many.
 *
 *----------------------------------------------------------------------
 */
static void CgiToClientBuffer(FastCgiInfo *infoPtr)
{
    FCGI_Header header;
    int len;

    while(BufferLength(infoPtr->inbufPtr) > 0) {

    /*
     * State #1:  looking for the next complete packet header.
     */
	if(infoPtr->gotHeader == FALSE) {
	    if(BufferLength(infoPtr->inbufPtr) < sizeof(FCGI_Header))
		return;
    
	    BufferGetData(infoPtr->inbufPtr, (char *) &header, 
		    sizeof(FCGI_Header));
    
	    /*
	     * XXX: Better handling of packets with other version numbers
	     * and other packet problems.
	     */
	    ASSERT(header.version == FCGI_VERSION);
	    ASSERT(header.type <= FCGI_MAXTYPE);

	    infoPtr->packetType = header.type;
    	    infoPtr->dataLen = (header.contentLengthB1 << 8) + header.contentLengthB0; 
	    infoPtr->gotHeader = TRUE;
	    infoPtr->paddingLen = header.paddingLength;
	}
    
	/*
	 * State #2:  got a header, and processing packet bytes.
	 */
	len = min(infoPtr->dataLen, BufferLength(infoPtr->inbufPtr));
	ASSERT(len >= 0);
	switch(infoPtr->packetType) {
    
	    case FCGI_STDOUT:
		if(len > 0) {
		    if(infoPtr->parseHeader)
			BufferDStringAppend(infoPtr->header, 
				infoPtr->inbufPtr, len);
		    else {
			len = min(BufferFree(infoPtr->reqOutbufPtr), len);
			if (len > 0)
			  BufferMove(infoPtr->reqOutbufPtr, infoPtr->inbufPtr, len);
			else
			  return;
		    }
		    infoPtr->dataLen -= len;
		}
		break;
    
	    case FCGI_STDERR:
		if(len > 0) {
		    BufferDStringAppend(infoPtr->errorOut, infoPtr->inbufPtr, len);
		    infoPtr->dataLen -= len;
		}
		break;

	      case FCGI_END_REQUEST:
		if(!infoPtr->readingEndRequestBody) {
		  if(infoPtr->dataLen != sizeof(FCGI_EndRequestBody)) {
		    char error_msg[MAX_STRING_LEN];
		    sprintf(error_msg, "HTTPd: FastCgi protocol error - End request packet size %d != FCGI_EndRequestBody size", infoPtr->dataLen);
		    Die(infoPtr,SERVER_ERROR,error_msg);
		  }
		  infoPtr->readingEndRequestBody = TRUE;
		}
		BufferMove(infoPtr->erBufPtr, infoPtr->inbufPtr, len);
		infoPtr->dataLen -= len;		
		if(infoPtr->dataLen == 0) {
		  FCGI_EndRequestBody *erBody = &infoPtr->endRequestBody;
		  BufferGetData(infoPtr->erBufPtr, (char *) &infoPtr->endRequestBody, 
				sizeof(FCGI_EndRequestBody));
		  if(erBody->protocolStatus != FCGI_REQUEST_COMPLETE) {
		    char error_msg[MAX_STRING_LEN];
		    sprintf(error_msg, "HTTPd: FastCgi protocol error -  end request status != FCGI_REQUEST_COMPLETE");
		    Die(infoPtr,SERVER_ERROR,error_msg);
		    /*
		     * XXX: What to do with FCGI_OVERLOADED?
		     */
		  }
		  infoPtr->exitStatus = (erBody->appStatusB3 << 24)
		    + (erBody->appStatusB2 << 16)
		      + (erBody->appStatusB1 <<  8)
			+ (erBody->appStatusB0 );
		  infoPtr->exitStatusSet = TRUE;
		  infoPtr->readingEndRequestBody = FALSE;
		}
		break;
	      case FCGI_GET_VALUES_RESULT:
		/* coming soon */
	      case FCGI_UNKNOWN_TYPE:
		/* coming soon */

		/*
		 * Ignore unknown packet types from the FastCGI server.
		 */
	    default:
		BufferToss(infoPtr->inbufPtr, len);
		infoPtr->dataLen -= len;	    
		break;
	}
    
	if (infoPtr->dataLen == 0) {
	  if (infoPtr->paddingLen > 0) {
	    len = min(infoPtr->paddingLen, BufferLength(infoPtr->inbufPtr));	    
	    BufferToss(infoPtr->inbufPtr, infoPtr->paddingLen);
	    infoPtr->paddingLen -= len;
	  }
	  /*
	   * If we're done with the data in the packet, then start looking for 
	   * the next header.
	   */
	  if (infoPtr->paddingLen <= 0)
	    infoPtr->gotHeader = FALSE;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ScanLine --
 *
 *	Terminate a line:  scan to the next newline, scan back to the
 *	first non-space character and store a terminating zero.  Return
 *	the next character past the end of the newline.
 *
 *	If the end of the string is reached, return a pointer to the
 *	end of the string.
 *
 *	If continuation is set to 'TRUE', then it parses a (possible)
 *	sequence of RFC-822 continuation lines.
 *
 * Results:
 *      As above.
 *
 * Side effects:
 *      Termination byte stored in string.
 *
 *----------------------------------------------------------------------
 */
char *ScanLine(char *start, int continuation)
{
    char *p = start;
    char *end = start;

    if(*p != '\n') {
	if(continuation) {
	    while(*p != '\0') {
		if(*p == '\n' && p[1] != ' ' && p[1] != '\t')
		    break;
		p++;
	    }
	} else {
	    while(*p != '\0' && *p != '\n')
		p++;
	}
    }

    end = p;
    if(*end != '\0')
	end++;

    /*
     * Trim any trailing whitespace.
     */
    while(isspace(p[-1]) && p > start)
	p--;

    *p = '\0';
    return end;
}

/*    
 *----------------------------------------------------------------------
 *
 * HTTPTime --
 *
 *	Return the current time, in HTTP time format.
 *
 * Results:
 *	Returns pointer to time string, in static allocated array.
 *
 * Side effects:
 *      None.
 *      
 *----------------------------------------------------------------------
 */
#define TIMEBUFSIZE 256
char *HTTPTime(struct tm *when)
{
    static char str[TIMEBUFSIZE];

    strftime(str, TIMEBUFSIZE-1, "%A, %d-%b-%y %T GMT", when);
    return str;
}
#undef TIMEBUFSIZE

/*
 *----------------------------------------------------------------------
 *
 * SendHeader --
 *
 *	Queue an HTTP header line for sending back to the client.  
 *	If this is an old HTTP request (HTTP/0.9) or an inner (nested)
 *	request, ignore and return.
 *
 *	NOTE:  This routine assumes that the sent data will fit in 
 *	       remaining space in the output buffer.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Header information queued to be sent to client.
 *
 *----------------------------------------------------------------------
 */
void SendHeader(FastCgiInfo *reqPtr, ...)
{
    va_list argList;
    char *str;

    va_start(argList, reqPtr);
    while (TRUE) {
	str = va_arg(argList, char *);
	if(str == NULL)
	    break;
	BufferAdd(reqPtr->reqOutbufPtr, str);
	ASSERT(BufferFree(reqPtr->reqOutbufPtr) > 0);
    }
    BufferAdd(reqPtr->reqOutbufPtr, "\r\n");	    /* terminate */
    ASSERT(BufferFree(reqPtr->reqOutbufPtr) > 0);
    va_end(argList);
}

/*
 *----------------------------------------------------------------------
 *
 * AddHeaders --
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
void AddHeaders(FastCgiInfo *reqPtr, char *str)
{
  BufferAdd(reqPtr->reqOutbufPtr, str);
}

/*
 *----------------------------------------------------------------------
 *
 * BeginHeader --
 *
 *	Begin a standard HTTP header:  emits opening message
 *	(i.e. "200 OK") and standard header matter.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
void BeginHeader(FastCgiInfo *reqPtr, char *msg)
{
    time_t now;

    ASSERT(BufferLength(reqPtr->reqOutbufPtr) == 0);

    now = time(NULL);
    SendHeader(reqPtr, SERVER_PROTOCOL, " ", msg, NULL);
    SendHeader(reqPtr, "Date: ", HTTPTime(gmtime(&now)), NULL);
    SendHeader(reqPtr, "Server: ", SERVER_VERSION, NULL);
/*    SendHeader(reqPtr, "MIME-version: 1.0", NULL); */

    /*
     * We shouldn't have run out of space in the output buffer.
     */
    ASSERT(BufferFree(reqPtr->reqOutbufPtr) > 0);
}

/*
 *----------------------------------------------------------------------
 *
 * EndHeader --
 *
 *	Marks the end of the HTTP header:  sends a blank line.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
void EndHeader(FastCgiInfo *reqPtr)
{
    SendHeader(reqPtr, "", NULL);
}
void ComposeURL(WS_Request *reqPtr, char *url, DString *result)
{
    if(url[0] == '/') {
      char portStr[10];
      if (HostInfo(reqPtr) && HostName(reqPtr)) {
	sprintf(portStr, "%d", HostPort(reqPtr));
	DStringAppend(result, "http://", -1);
	DStringAppend(result, HostName(reqPtr), -1);	
	DStringAppend(result, ":", -1);
	DStringAppend(result, portStr, -1);
      }
    }
    DStringAppend(result, url, -1);
}
/*
 *----------------------------------------------------------------------
 *
 * HT_ScanCGIHeader --
 *
 *	Scans the CGI header data to see if the CGI program has sent a 
 *	complete header.  If it has, then parse the header and queue
 *	the HTTP response header information back to the client.
 *
 * Results:
 *	TRUE if completed without error, FALSE otherwise.
 *
 * Side effects:
 *	Sets 'reqPtr->parseHeader' to TRUE if the header was parsed
 *	successfully.
 *
 *----------------------------------------------------------------------
 */
int ScanCGIHeader(WS_Request *reqPtr, FastCgiInfo *infoPtr)
{
    DString status;
    DString headers;
    DString cgiLogName;
    char *line, *next, *p, *data, *name;
    int len, flag, sent;
    int bufFree;

    ASSERT(infoPtr->parseHeader == TRUE);

    /*
     * Do we have the entire header?  Scan for the blank line that
     * terminates the header.
     */
    p = DStringValue(infoPtr->header);
    len = DStringLength(infoPtr->header);
    flag = 0;
    while(len-- && flag < 2) {
	switch(*p) {
	    case '\r':  
		break;
	    case '\n':
		flag++;
		break;
	    default:
		flag = 0;
		break;
	}
	p++;
    }

    /*
     * Return (to be called later when we have more data) if we don't have
     * and entire header.
     */
    if(flag < 2)
	return TRUE;

    infoPtr->parseHeader = FALSE;

    DStringInit(&status);
    DStringAppend(&status, "200 OK", -1);
    DStringInit(&headers);
    DStringInit(&cgiLogName);
    next = DStringValue(infoPtr->header);
    for(;;) {
	next = ScanLine(line = next, TRUE);
	if(*line == '\0')
	    break;

        if((p = strpbrk(line, " \t")) != NULL) {
            data = p + 1;
            *p = '\0';
        } else
            data = "";

	while(isspace(*data))
	    data++;

	/*
	 * Handle "Location:", "Status:", and "Log-XXXX" specially.
	 * All other CGI headers get passed through unmodified.
	 */
	if(!strcasecmp(line, "Location:")) {
	    DStringTrunc(&status, 0);
	    DStringAppend(&status, "302 Redirect", -1);
	    DStringAppend(&headers, "Location: ", -1);

	    /*
	     * This is a deviation from the CGI/1.1 spec.
	     *
	     * The spec says that "virtual paths" should be treated
	     * as if the client had accessed the file in the first
	     * place.  This usually breaks relative references in the
	     * referenced document.
	     *
	     * XXX: do more research on this?
	     */
	    ComposeURL(reqPtr, data, &headers);
	    DStringAppend(&headers, "\r\n", -1);
	} else if(!strcasecmp(line, "Status:")) {
	    DStringTrunc(&status, 0);
	    DStringAppend(&status, data, -1);
	} else if(Tcl_StringMatch(line, "[Ll][Oo][Gg]-?*:")) {
	    p = strchr(line, '-');
	    ASSERT(p != NULL);
	    name = p + 1;

	    p = strchr(name, ':');
	    ASSERT(p != NULL);
	    *p = '\0';

	    DStringTrunc(&cgiLogName, 0);
	    DStringAppend(&cgiLogName, "cgi-", -1);
	    DStringAppend(&cgiLogName, name, -1);
	} else {
	    if(data != NULL) {
		DStringAppend(&headers, line, -1);
		DStringAppend(&headers, " ", 1);
		DStringAppend(&headers, data, -1);
		DStringAppend(&headers, "\r\n", -1);
	    }
	}
    }

    /*
     * We're done scanning the CGI script's header output.  Now
     * we have to write to the client:  status, CGI header, and
     * any over-read CGI output.
     */
    BeginHeader(infoPtr, DStringValue(&status));
    DStringFree(&status);

    AddHeaders(infoPtr, DStringValue(&headers));
    DStringFree(&headers);
    DStringFree(&cgiLogName);
    EndHeader(infoPtr);

    len = next - DStringValue(infoPtr->header);
    len = DStringLength(infoPtr->header) - len;
    ASSERT(len >= 0);

    bufFree = BufferFree(infoPtr->reqOutbufPtr);

    if (bufFree < len) {
      /* should the same fix be made in core server? */
      int bufLen = BufferLength(infoPtr->reqOutbufPtr);
      Buffer *newBuf = BufferCreate(len + bufLen);
      BufferMove(newBuf, infoPtr->reqOutbufPtr, bufLen);
      BufferDelete(infoPtr->reqOutbufPtr);
      infoPtr->reqOutbufPtr = newBuf;
    }
    bufFree = BufferFree(infoPtr->reqOutbufPtr);
    if(bufFree == 0)
        goto fail;
       
    /*
     * Only send the body for methods other than HEAD.
     */
    if(reqPtr->method != M_HEAD) {
        if(len > 0) {
            sent = BufferAddData(infoPtr->reqOutbufPtr, next, len);
            if(sent != len)
                goto fail;
        }
    }
    return TRUE;

fail:
    return FALSE;
}

/*
 *----------------------------------------------------------------------
 * 
 * FastCgiDoWork --
 *
 *      This is the core routine for moving data between the CGI program
 *      and the network socket.  
 * 
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	many
 * 
 *----------------------------------------------------------------------
 */
static int FastCgiDoWork(WS_Request *reqPtr, FastCgiInfo *infoPtr)
{
  fd_set read_set, write_set;
  int numFDs, select_status;
  struct timeval timeOut;
  int clientRead, fcgiRead;
  int nBytes;
  int csdIn, csdOut;
  int contentLength;
  int nFirst = 1;
  int done = 0;
  clientRead = fcgiRead = 1;
  FD_ZERO(&read_set);
  FD_ZERO(&write_set);
  numFDs = infoPtr->fd + 1;
  timeOut.tv_sec = timeOut.tv_usec = 0;
  contentLength = reqPtr->inh_content_length;
  if (contentLength <= 0)
    clientRead = 0;
  PrepareClientSocket(reqPtr, &csdIn, &csdOut);
  while (!done) {

    if(BufferLength(infoPtr->outbufPtr))
      FD_SET(infoPtr->fd, &write_set);
    else
      FD_CLR(infoPtr->fd, &write_set);

    FD_SET(infoPtr->fd, &read_set);
    FD_SET(csdIn, &read_set);

    if(BufferLength(infoPtr->reqOutbufPtr))
      FD_SET(csdOut, &write_set);
    else
      FD_CLR(csdOut, &write_set);

    select_status = select(numFDs, &read_set, &write_set, NULL, NULL);
    if(select_status < 0)
      break;

    /*
     * Move data from the FastCGI program's standard output to the network
     * socket, until we can do no more work without blocking.
     */
    ClientToCgiBuffer(infoPtr);
    CgiToClientBuffer(infoPtr);

    if(infoPtr->parseHeader == TRUE) {
        if(ScanCGIHeader(reqPtr, infoPtr) == FALSE) {
	    char error_msg[MAX_STRING_LEN];
	    sprintf(error_msg,"HTTPd: malformed header from script %s",
		    reqPtr->filename);
            Die(infoPtr,SERVER_ERROR,error_msg);
            return SERVER_ERROR;
        }
    }

    if (nFirst) {
      char szBuf[IOBUFSIZE];
      nBytes=getline(reqPtr->sb, szBuf,IOBUFSIZE,G_FLUSH,0);
      BufferAddData(infoPtr->reqInbufPtr, szBuf, nBytes);
      if (nBytes > 0) {
	BufferAddData(infoPtr->reqInbufPtr, szBuf, nBytes);
	ClientToCgiBuffer(infoPtr);
      }
      nFirst = 0;
      contentLength -= nBytes;
      if (contentLength <= 0)
	clientRead = 0;
    }

    /*
     * Move data between the buffers and the network sockets.
     */
    if(FD_ISSET(csdIn, &read_set) &&
       (BufferFree(infoPtr->reqInbufPtr) > 0)) {
      nBytes = BufferRead(infoPtr->reqInbufPtr, csdIn);
      if (nBytes >= 0)
	contentLength -= nBytes;
      if (contentLength <= 0)
	clientRead = 0;
    }

    if(FD_ISSET(infoPtr->fd, &read_set) && 
       (BufferFree(infoPtr->inbufPtr) > 0) &&
       infoPtr->fd >= 0) {
      if (BufferRead(infoPtr->inbufPtr, infoPtr->fd) <= 0)
	fcgiRead = 0;
    }

    if(FD_ISSET(csdOut, &write_set) &&
       (BufferLength(infoPtr->reqOutbufPtr) > 0)) {
      BufferWrite(infoPtr->reqOutbufPtr, csdOut);
    }

    if(FD_ISSET(infoPtr->fd, &write_set) &&
       (BufferLength(infoPtr->outbufPtr) > 0) &&
       infoPtr->fd >= 0) {
      BufferWrite(infoPtr->outbufPtr, infoPtr->fd);
    }
    if (fcgiRead)
      fcgiRead = (!infoPtr->exitStatusSet);
    if (((clientRead == 0) ||
	 !FD_ISSET(csdIn, &read_set)) &&
	(fcgiRead == 0) && 
	(BufferLength(infoPtr->reqOutbufPtr) <= 0) &&
	(BufferLength(infoPtr->outbufPtr) <= 0)) {
      if (infoPtr->parseHeader == TRUE) {
	char error_msg[MAX_STRING_LEN];
	sprintf(error_msg,"HTTPd: malformed header from script %s",
		reqPtr->filename);
	Die(infoPtr,SERVER_ERROR,error_msg);
      }
      done = 1;
    }
  }
  FcgiCleanUp(infoPtr);
  reqPtr->outh_content_length = -1;
  return OK;
}


/*
 *----------------------------------------------------------------------
 * 
 * ConnectionComplete --
 *
 *	This routine gets called when the connection completes.  If
 *	the connection completed with an error, it generates an error
 *	result back to the client.  Otherwise, it begins a FastCGI
 *	request (sending the environment variables).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Request terminated, or continued.
 * 
 *----------------------------------------------------------------------
 */
static int ConnectionComplete(WS_Request *reqPtr, FastCgiInfo *infoPtr)
{
    int errorCode, len;
    FastCgiServerInfo *serverInfoPtr = infoPtr->serverPtr;

    /*
     * Get the connection status.
     */
    len = sizeof(errorCode);
    if(getsockopt(infoPtr->fd, SOL_SOCKET, SO_ERROR,
                  (char *) &errorCode, &len) < 0) {
	ConnectionError(infoPtr, errno);
	return SERVER_ERROR;
    }
    ASSERT(len == sizeof(errorCode));
    if(errorCode > 0) {
	ASSERT(errorCode != EINPROGRESS);
	ConnectionError(infoPtr, errorCode);
	return SERVER_ERROR;
    }
     
/* Don't do keepalive unless the script returns a content-length
 * header 
 */
    keep_alive.bKeepAlive = 0;
     
    SendBeginRequest(infoPtr);
    SendEnvironment(reqPtr, infoPtr);
    return FastCgiDoWork(reqPtr, infoPtr);
}



/*
 *----------------------------------------------------------------------
 * 
 * FastCgiHandler --
 *
 *	This routine gets called for a request that corresponds to
 *	a Fast CGI connection.  It begins the connect operation to the
 *	FastCGI server.  
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Connection to FastCGI server established.
 * 
 *----------------------------------------------------------------------
 */
int FastCgiHandler(WS_Request *reqPtr)
{
    FastCgiServerInfo *serverInfoPtr;
    FastCgiInfo *infoPtr;
    int scriptTimeout;
    OS_IpcAddr *ipcAddrPtr;
    struct stat finfo;

    get_path_info(reqPtr,&finfo);

    SetErrorLogFd(reqPtr, 0);

    serverInfoPtr = FastCgiServerInfoLookup(reqPtr->filename);

    /*
     * Allocate FastCGI private data to hang off of the request structure.
     */
    infoPtr = (FastCgiInfo *) Malloc(sizeof(FastCgiInfo));
    infoPtr->serverPtr = serverInfoPtr;
    infoPtr->inbufPtr = BufferCreate(SERVER_BUFSIZE);
    infoPtr->outbufPtr = BufferCreate(SERVER_BUFSIZE);
    infoPtr->gotHeader = FALSE;
    infoPtr->reqInbufPtr = BufferCreate(SERVER_BUFSIZE);
    infoPtr->reqOutbufPtr = BufferCreate(SERVER_BUFSIZE);
    infoPtr->parseHeader = TRUE;
    infoPtr->header = (DString *) malloc(sizeof(DString));
    infoPtr->errorOut = (DString *) malloc(sizeof(DString));
    infoPtr->reqPtr = reqPtr;

    DStringInit(infoPtr->header);
    DStringInit(infoPtr->errorOut);
    infoPtr->erBufPtr = BufferCreate(sizeof(FCGI_EndRequestBody) + 1);
    infoPtr->readingEndRequestBody = FALSE;
    infoPtr->exitStatus = 0;
    infoPtr->exitStatusSet = FALSE;
    infoPtr->requestId = 1; /* XXX need to set to some meaningful value */
    globalInfoPtr = infoPtr;

    /* 
     * Don't core dump if the server isn't configured 
     */
    if (serverInfoPtr == NULL)
      ConnectionError(infoPtr,0);

    /*
     * synchronously open a connection to Fast CGI server.  
     */
    ipcAddrPtr = (OS_IpcAddr *) serverInfoPtr->ipcAddr;
    if((infoPtr->fd = OS_Socket(ipcAddrPtr->serverAddr->sa_family, 
	SOCK_STREAM, 0)) < 0) {
	    ConnectionError(infoPtr, errno);
	    return SERVER_ERROR;
    }
    if(connect(infoPtr->fd, (struct sockaddr *) ipcAddrPtr->serverAddr,
	ipcAddrPtr->addrLen) < 0) {
	    ConnectionError(infoPtr, errno);
	    return SERVER_ERROR;
    }
    return ConnectionComplete(reqPtr, infoPtr);
}


/*
 * OS_CreateLocalIpcFd --
 *
 *   This procedure is responsible for creating the listener socket
 *   on Unix for local process communication.  It will create a Unix
 *   domain socket, bind it, and return a file descriptor to it to the
 *   caller.
 *
 * Results:
 *      Listener socket created.  This call returns either a valid
 *      file descriptor or -1 on error.
 *
 * Side effects:
 *      Listener socket and IPC address are stored in the FCGI info
 *      structure.
 *
 *----------------------------------------------------------------------
 */
int OS_CreateLocalIpcFd(OS_IpcAddress ipcAddress, int listenQueueDepth)
{
    OS_IpcAddr *ipcAddrPtr = (OS_IpcAddr *)ipcAddress;
    char bindPathExt[32];
    struct sockaddr_un *addrPtr;
    int listenSock = -1;
    addrPtr = NULL;

    /*
     * Create a unique name to be used for the socket bind path.
     * Make sure that this name is unique and that there's no process
     * bound to it.
     *
     * Bind Path = /tmp/OM_WS_N.pid
     *
     */
    ASSERT(DStringLength(&ipcAddrPtr->bindPath) == 0);
    DStringAppend(&ipcAddrPtr->bindPath, "/tmp/OM_WS_", -1);
    sprintf(bindPathExt, "%d.%d", bindPathExtInt, (int)getpid());
    bindPathExtInt++;
    DStringAppend(&ipcAddrPtr->bindPath, bindPathExt, -1);
    
    /*
     * Build the domain socket address.
     */
    addrPtr = (struct sockaddr_un *) Malloc(sizeof(struct sockaddr_un));
    ipcAddrPtr->serverAddr = (struct sockaddr *) addrPtr;

    if (OS_BuildSockAddrUn(DStringValue(&ipcAddrPtr->bindPath), addrPtr,
			   &ipcAddrPtr->addrLen)) {
	goto GET_IPC_ERROR_EXIT;
    }

    /*
     * Create the listening socket to be used by the fcgi server.
     */
    if((listenSock = OS_Socket(ipcAddrPtr->serverAddr->sa_family,
			       SOCK_STREAM, 0)) < 0) {
	goto GET_IPC_ERROR_EXIT;
    }

    /*
     * Bind the lister socket and set it to listen.
     */
    if(OS_Bind(listenSock, ipcAddrPtr->serverAddr, ipcAddrPtr->addrLen) < 0
       || OS_Listen(listenSock, listenQueueDepth) < 0) {
	goto GET_IPC_ERROR_EXIT;
    }
    return listenSock;

GET_IPC_ERROR_EXIT:
    if(listenSock != -1)
        OS_Close(listenSock);
    if(addrPtr != NULL) {
        free(addrPtr);
	ipcAddrPtr->serverAddr = NULL;
    }
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * OS_FreeIpcAddr --
 *
 *	Free up and clean up an OS IPC address.
 *
 * Results:
 *      IPC Address is freed.
 *
 * Side effects:  
 *      More memory.
 *
 *----------------------------------------------------------------------
 */  
void OS_FreeIpcAddr(OS_IpcAddress ipcAddress)
{
    OS_IpcAddr *ipcAddrPtr = (OS_IpcAddr *)ipcAddress;
    
    DStringFree(&ipcAddrPtr->bindPath); 
    if(ipcAddrPtr->serverAddr != NULL) 
        free(ipcAddrPtr->serverAddr);
    ipcAddrPtr->addrLen = 0;
    free(ipcAddrPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * OS_ExecFcgiProgram --
 *
 *	Fork and exec the specified fcgi process.
 *
 * Results:
 *      FCGI Program is forked and should be ready to accept
 *      connections.
 *      Returns 0 on success or a valid server error on failure.
 *
 * Side effects:  
 *      Child process is transformed into the program.  Sets errno
 *      on error.
 *
 *----------------------------------------------------------------------
 */
int OS_ExecFcgiProgram(pid_t *childPid, int listenFd, int priority,
		       char *programName, char **envPtr)
{
    int i;
    DString dirName;
    char *dnEnd;
    
    /*
     * Fork the fcgi process.
     */
    *childPid = fork();
    if(*childPid < 0) {
        return SERVER_ERROR;
    }
    if(*childPid == 0) {
        if(listenFd != FCGI_LISTENSOCK_FILENO) {
            OS_Dup2(listenFd, FCGI_LISTENSOCK_FILENO);
	    OS_Close(listenFd);
	}

	DStringInit(&dirName);
	dnEnd = strrchr(programName, '/');
	if(dnEnd == NULL) {
	    DStringAppend(&dirName, "./", 1);
	} else {
	    DStringAppend(&dirName, programName, dnEnd - programName);
	}

        /*
	 * We're a child.  Exec the application.
	 */
	if(chdir(DStringValue(&dirName)) < 0) {
	    fprintf(errorLogFd, "exec fcgi: failed to change dir %s -- %s -- %s\n",
		    programName, DStringValue(&dirName), strerror(errno));
 	    exit(1);
	}
	DStringFree(&dirName);
	
        /*
         * Set priority of the process.
         */
	if (priority != 0) {
	    if (nice (priority) == -1) {
	        fprintf (errorLogFd, "exec fcgi: failed to change priority -- %s -- %s\n",
			 programName, strerror(errno));
		exit (1);
	    }
	}

	/*
	 * Close any file descriptors we may have gotten from the parent
	 * process.  The only FD left open is the FCGI listener socket.
	 */
	for(i=0; i < ht_openmax; i++) {
	    if(i != FCGI_LISTENSOCK_FILENO)
	        OS_Close(i);
	}

	if(envPtr != NULL) {
	    execle(programName, programName, NULL, envPtr);
	}else {
	    execl(programName, programName, NULL);
	}
	exit(errno);
	}
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * OS_CleanupFcgiProgram --
 *
 *	Perform OS cleanup on a server managed process.
 *
 * Results:
 *      None.
 *
 * Side effects:  
 *      Domain socket bindPath is unlinked.
 *
 *----------------------------------------------------------------------
 */  
void OS_CleanupFcgiProgram(OS_IpcAddress ipcAddress)
{
    OS_IpcAddr *ipcAddrPtr = (OS_IpcAddr *)ipcAddress;
    unlink(DStringValue(&ipcAddrPtr->bindPath));
}

char *SkipSpace(char *inp)
{
  while (*inp == ' ')
    inp++;
  return inp;
}

char **ParseAppClassArgs(char *inp, int *ac) 
{
  char *input = (char *) malloc(strlen(inp) + 1);
  char *ptr = input;
  char *prev = ptr;
  int count = 0;
  char **argv = NULL;
  strcpy(input, inp);

  while ((ptr = strchr(prev, ' '))) {
    ptr++;
    count++;
    ptr = SkipSpace(ptr);
    prev = ptr;
  }
  if (*prev) count++;
  argv = (char **) malloc(sizeof(char *) * (count + 1));
  count = 1;
  prev = input;
  while ((ptr = strchr(prev, ' '))) {
    *ptr++ = 0;
    argv[count] = prev;
    count++;
    ptr = SkipSpace(ptr);
    prev = ptr;
  }
  if (*prev) {
    argv[count] = prev;
    count++;
  }
  *ac = count;
  return argv;
}

/*
 *-------------------------------------------------------
 * OS_EnvironInit
 *
 *
 *
 *------------------------------------------------------
 */

char **OS_EnvironInit(int envCount)
{
  int i;
  char **envPtr = (char **) Malloc(sizeof(char *) * envCount);
  for (i = 0; i < envCount; ++i)
    envPtr[i] = NULL;
  return envPtr;
}

void OS_EnvironFree(char **envPtr)
{
  int i;
  char **tmp = envPtr;
  while (*tmp) {
    free(*tmp);
    tmp++;
  }
}

void OS_EnvString(char **envPtr, char *name, char *value)
{
    char *buf;
    int size;
    buf = (char *)Malloc(strlen(name) + strlen(value) + 2);
    sprintf(buf, "%s=%s", name, value);
    *envPtr = buf;
}


/*
 *----------------------------------------------------------------------
 *
 * CreateFcgiServerInfo --
 *
 *	This routine allocates and initializes a fast cgi server info
 *      structure.  It's called from AppClass, ExternalAppClass and
 *      __SendFcgiScript.  This routine is responsible for adding the
 *      new entry to the appClassTable also.
 *
 * Results:
 *	NULL pointer is returned if the class has already been defined
 *      or a valid fast cgi server info pointer.
 *
 * Side effects:
 *	Fast cgi server info structure is allocated and initialized.
 *      This includes allocation and initialization of the per 
 *      connection information.
 *
 *----------------------------------------------------------------------
 */
static FastCgiServerInfo * CreateFcgiServerInfo(int numInstances,
						char *ePath)
{
    FastCgiServerInfo *serverInfoPtr = NULL;
    FcgiProcessInfo *procInfoPtr;
    int i, new;

    serverInfoPtr = FastCgiServerInfoLookup(ePath);
    if (serverInfoPtr)
      return NULL;

    /*
     * Create an info structure for the Fast CGI server (TCP type).
     */
    serverInfoPtr = (FastCgiServerInfo *) Malloc(sizeof(FastCgiServerInfo));
    DStringInit(&serverInfoPtr->execPath);
    serverInfoPtr->envp = NULL;
    serverInfoPtr->listenQueueDepth = DEFAULT_FCGI_LISTEN_Q;
    serverInfoPtr->maxProcesses = numInstances;
    serverInfoPtr->restartDelay = 0;
    serverInfoPtr->restartOnExit = FALSE;
    serverInfoPtr->numRestarts = 0;
    serverInfoPtr->numFailures = 0;
    serverInfoPtr->ipcAddr = OS_InitIpcAddr();
    serverInfoPtr->processPriority = 0;
    serverInfoPtr->listenFd = -1;
    serverInfoPtr->reqRefCount = 0;
    serverInfoPtr->freeOnZero = FALSE;
    serverInfoPtr->restartTimerQueued = FALSE;
    /*
     * XXX: For initial testing of the new protocol, close the connection
     *      after each request.  Once debugged, we will query the app
     *      to see whether or not it can maintain a persistent connection
     *      based on the number of connections it can support and the
     *      number of processes we have running.
     */
    serverInfoPtr->keepConnection = FALSE;
    serverInfoPtr->fcgiFd = -1;
    
    serverInfoPtr->procInfo = 
      (FcgiProcessInfo *) Malloc(sizeof(FcgiProcessInfo) * numInstances);

    procInfoPtr = serverInfoPtr->procInfo;
    for(i = 0; i < numInstances; i++) {
        procInfoPtr->pid = -1;
	procInfoPtr->listenFd = -1;
	procInfoPtr->fcgiFd = -1;
        procInfoPtr->ipcAddr = OS_InitIpcAddr();
	procInfoPtr->serverInfoPtr = serverInfoPtr;
	procInfoPtr++;
    }
    serverInfoPtr->next = fastCgiServers;
    fastCgiServers = serverInfoPtr;
    return serverInfoPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * FreeFcgiServerInfo --
 *
 *	This routine frees up all resources associated with a Fast CGI
 *      application server.  It's called on error cleanup and as a result
 *      of a shutdown or restart.
 *
 * Results:
 *	FastCgi server and process structures freed.
 *
 * Side effects:
 *	Fast cgi info structure is deallocated and unavailable.
 *
 *----------------------------------------------------------------------
 */
static void FreeFcgiServerInfo(FastCgiServerInfo *serverInfoPtr)
{
    FcgiProcessInfo *processInfoPtr;
    int i;

    /*
     * If there are any external references to this server structure
     * set the "freeOnZero" flag so that the structure will be freed
     * when both the request and role reference counts go to zero.
     *
     * The freeOnZero flag is set:
     *
     * o unconditionally for .fcgi programs.  This is because a .fcgi
     *   program does not create a persistent process.  This type of
     *   program only lives for a single request.
     *
     * o server shutdown that causes FreeRoleInfo to be called because
     *   mount handlers are being cleaned up.  The FreeRoleInfo call
     *   calls this routine (FreeFcgiServerInfo) unconditionally.
     */
    if(serverInfoPtr->reqRefCount != 0) {
        serverInfoPtr->freeOnZero = TRUE;
        return;
    }

    /*
     * Free up process/connection info.
     */
    processInfoPtr = serverInfoPtr->procInfo;
    for(i = 0; i < serverInfoPtr->maxProcesses; i ++, processInfoPtr++) {
	if(processInfoPtr->pid != -1) {
	    kill(processInfoPtr->pid, SIGTERM);
	    processInfoPtr->pid = -1;
	}
	OS_FreeIpcAddr(processInfoPtr->ipcAddr);
    }
    /*
     * Cleanup server info structure resources.
     */
    OS_CleanupFcgiProgram(serverInfoPtr->ipcAddr);
    OS_FreeIpcAddr(serverInfoPtr->ipcAddr);
    DStringFree(&serverInfoPtr->execPath);
    if(serverInfoPtr->listenFd != -1) {
        OS_Close(serverInfoPtr->listenFd);
	serverInfoPtr->listenFd = -1;
    }

    free(serverInfoPtr->procInfo);
    serverInfoPtr->procInfo = NULL;

    if(serverInfoPtr->envp != NULL) {
	OS_EnvironFree(serverInfoPtr->envp);
	free(serverInfoPtr->envp);
	serverInfoPtr->envp = NULL;
    }
    /* XXX remove serverInfoPtr from chain */

    if (serverInfoPtr == fastCgiServers) 
        fastCgiServers = fastCgiServers->next;
    else {
        FastCgiServerInfo *tmpPtr = fastCgiServers;
	while (tmpPtr->next && (tmpPtr->next != serverInfoPtr) )
	    tmpPtr = tmpPtr->next;
	if (tmpPtr->next == serverInfoPtr)
	    tmpPtr->next = serverInfoPtr->next;
    }
    free(serverInfoPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * FcgiProgramExit --
 *
 *	This routine gets called when the child process for the
 *	program exits.  The exit status is recorded in the request log.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Status recorded in log, pid set to -1.
 *
 *----------------------------------------------------------------------
 */
static void FcgiProgramExit(FcgiProcessInfo *processInfoPtr, int status)
{
    FastCgiServerInfo *serverInfoPtr = processInfoPtr->serverInfoPtr;
    time_t restartTime, timeNow;

    serverInfoPtr->numFailures++;
    processInfoPtr->pid = -1;
    /*
     * Only log the state of the process' exit if it's expected to stay
     * alive.
     *
     * XXX: MAY want to have a different process exit routine for .fcgi
     */
    if(serverInfoPtr->restartOnExit == TRUE) {
        if(WIFEXITED(status) && (WEXITSTATUS(status) != 0)) {
	    fprintf(errorLogFd, "FastCGI process '%s' terminated due to exit",
		    DStringValue(&serverInfoPtr->execPath));
	} else if(WIFSIGNALED(status)) {
	    fprintf(errorLogFd, "FastCGI process '%s' terminated due to signal",
		    DStringValue(&serverInfoPtr->execPath));
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ExecFcgiProgram --
 *
 *	This procedure is invoked to start a fast cgi program.  It
 *      calls an OS specific routine to start the process and sets a
 *      wait handler on the process.
 *
 *      NOTE: At the present time, the only error than can be returned is
 *            SERVER_UNAVAILABLE which means a fork() failed (WS_errno is set).
 *
 * Results:
 *      Returns 0 if successful or a valid server error if not.
 *
 * Side effects:
 *      Possibly many.
 *
 *----------------------------------------------------------------------
 */
static int ExecFcgiProgram(FcgiProcessInfo *processInfoPtr, int priority)
{
    FastCgiServerInfo *serverInfoPtr = processInfoPtr->serverInfoPtr;
    int err = FALSE;
    int listenFd;

    listenFd = serverInfoPtr->listenFd;

    /*
     * The call to OS_ExecFcgiProgram will only return once.  It will not
     * return as a result of the child's creation.
     */

    if ((err = OS_ExecFcgiProgram(&processInfoPtr->pid, listenFd, priority,
				  DStringValue(&serverInfoPtr->execPath),
				  serverInfoPtr->envp)) != 0) {
	return err;
    }
    return 0;
}

static void FastCGIRestart()
{
    while (fastCgiServers) {
        FreeFcgiServerInfo(fastCgiServers);
    }
  exit(0);
}
static void FastCGISetSignals()
{
    signal(SIGTERM,(void (*)())FastCGIRestart);
    signal(SIGHUP,(void (*)())FastCGIRestart);
    signal(SIGKILL,(void (*)())FastCGIRestart);
}

/*
 *----------------------------------------------------------------------
 * StartFastCGIManager
 * 
 *    start the FastCGI process managers which is responsible for:
 *      - starting all the FastCGI proceses.
 *      - restart should any of these die
 *        - looping and call waitpid() on each child
 *      - clean up these processes when server restart
 *
 * Results:
 *
 * Side effects:
 *	Registers a new AppClass handler for FastCGI.
 *
 *----------------------------------------------------------------------
 */
void RunFastCGIManager(void *data)
{
    FastCgiServerInfo *serverInfoPtr = (FastCgiServerInfo *)data;

    int i;
    FcgiProcessInfo *processInfoPtr;
    FastCGISetSignals();
    processInfoPtr = serverInfoPtr->procInfo;
    for(i = 0; i < serverInfoPtr->maxProcesses; i++) {
        if(ExecFcgiProgram(processInfoPtr,
			   serverInfoPtr->processPriority) != 0) {
	    fprintf(errorLogFd, "AppClass: failed to exec app class %s\n",
		    DStringValue(&serverInfoPtr->execPath));
	    FreeFcgiServerInfo(serverInfoPtr);
	    exit(0); /* exit from procManager processes,
			should also clean up XXX
			*/
	}
	processInfoPtr++;
    }
    while (1) { /* looping to detect and reborn any dead child */
        int status;
	    
	sleep(serverInfoPtr->restartDelay);
	if (serverInfoPtr->restartOnExit == FALSE)
	    continue;
	processInfoPtr = serverInfoPtr->procInfo;
	for(i = 0; i < serverInfoPtr->maxProcesses; i++) {
	    int status;
	    if ((processInfoPtr->pid > 0) && 
		(waitpid(processInfoPtr->pid, &status, WNOHANG) > 0)) {
	        FcgiProgramExit(processInfoPtr, status);   
		if(ExecFcgiProgram(processInfoPtr,
				   serverInfoPtr->processPriority) != 0) {
		    fprintf(errorLogFd, "AppClass: failed to exec app class %s\n",
			    DStringValue(&serverInfoPtr->execPath));
		}
	    }
	    processInfoPtr++;
	}
    }
}

int StartFastCGIManager(FastCgiServerInfo *serverInfoPtr)
{
    pid_t procManager;
      
    procManager = SpawnChild(RunFastCGIManager, serverInfoPtr);
    if (procManager < 0) {
      return SERVER_ERROR;
    }
    serverInfoPtr->procManager = procManager;
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * AppClassCmd --
 *
 *	Implements the FastCGI AppClass configuration directive.  This
 *      command adds a fast cgi program class which will be used by the
 *      httpd parent to start/stop/maintain fast cgi server apps.
 *
 *      AppClass <exec-path> [-processes N] \
 *               [-restart-delay N] [-priority N] \
 *               [-initial-env name1=value1] \
 *               [-initial-env name2=value2]
 *
 * Default values:
 *
 * o numProcesses will be set to FCGI_MAX_PROCESSES (1)
 * o restartDelay will be set to 5 which means the application will not
 *   be restarted any earlier than 5 seconds from when it was last
 *   invoked.  If the application has been up for longer than 5 seconds
 *   and it fails, a single copy will be restarted immediately.  Other
 *   restarts within that group will be inhibited until the restart-delay
 *   has elapsed.
 * o affinity will be set to FALSE (ie. no process affinity) if not
 *   specified.
 *
 * Results:
 *	TCL_OK or TCL_ERROR.
 *
 * Side effects:
 *	Registers a new AppClass handler for FastCGI.
 *
 *----------------------------------------------------------------------
 */
char * AppClassCmd(per_host *host, char *arg)

{
    int numProcesses = -1;
    int affinity = 0;
    int restartDelay = FCGI_DEFAULT_RESTART_DELAY;
    char *execPath;
    char temp[200];
    FastCgiServerInfo *serverInfoPtr = NULL;
    FcgiProcessInfo *processInfoPtr;
    int i;
    int listenFd = -1;
    char *namePtr;
    char *valuePtr;
    char **envPtr = NULL;
    char **envHead;
    int priority = 0; /* Default is that of the Web Server */
    int listenQueueDepth = -1;
    int argc;
    char **argv;
    int envCount;
    int listenSock = -1;
    SetErrorLogFd(host, 1);

    argv = ParseAppClassArgs(arg, &argc);
    if(argc < 3) {
        fprintf(errorLogFd, "AppClass: too few args\n");
        return "AppClass: directive error";
    }
   /*
    * Parse and validate arguments.
    *
    * arg 1 is the name and arg 2 is the exec-path.
    */
    execPath = argv[1];

    serverInfoPtr = FastCgiServerInfoLookup(execPath);
    if (serverInfoPtr) {
      if (serverInfoPtr->procManager > 0)
	kill(serverInfoPtr->procManager, SIGHUP);
      FreeFcgiServerInfo(serverInfoPtr);
      serverInfoPtr = NULL;
    }

    envCount = argc/2 + 1;
    envPtr = OS_EnvironInit(envCount);
    envHead = envPtr;

    if (WS_Access(execPath, X_OK)) {
      fprintf(errorLogFd, "AppClass: Failed to access %s\n", execPath);
      goto APP_CLASS_ERROR;
    }

    for(i = 2; i < argc; i++) {
	if((strcmp(argv[i], "-processes") == 0)) {
	    if((i + 1) == argc) {
	        fprintf(errorLogFd, "AppClass: missing value for -processes\n");
		goto APP_CLASS_ERROR;
	    }
	    i++;
	    numProcesses = atoi(argv[i]);
	    if((numProcesses < 1) || (numProcesses > FCGI_MAX_PROCESSES)) {
	        fprintf(errorLogFd, "AppClass: -processes %d is out of range.  Valid range is 1 to %d",
			numProcesses, FCGI_MAX_PROCESSES);
		goto APP_CLASS_ERROR;
	    }
	    continue;
	} else if((strcmp(argv[i], "-restart-delay") == 0)) {
	    if((i + 1) == argc) {
	        fprintf(errorLogFd, "AppClass: missing value for -restart-delay\n");
		goto APP_CLASS_ERROR;
	    }
	    i++;
	    restartDelay = atoi(argv[i]);
	    if(restartDelay < 0) {
	        fprintf(errorLogFd, "AppClass: negative value for -restart-delay\n");
		goto APP_CLASS_ERROR;
	    }
	    continue;
	} else if((strcmp(argv[i], "-priority") == 0)) {
	    if((i + 1) == argc) {
	        fprintf(errorLogFd, "AppClass: missing value for -priority\n");
		goto APP_CLASS_ERROR;
	    }
	    i++;
	    priority = atoi(argv[i]);
	    if(priority < 0 || priority > 20) {
	        fprintf(errorLogFd, "AppClass: invalid value for -priority\n");
		goto APP_CLASS_ERROR;
	    }
	    continue;
	} else if((strcmp(argv[i], "-initial-env") == 0)) {
	    if((i + 1) == argc) {
	        fprintf(errorLogFd, "AppClass: missing value for -initial-env\n");
		goto APP_CLASS_ERROR;
	    }
	    i++;
	    namePtr = argv[i];
	    valuePtr = strchr(namePtr, '=');
	    /*
	     * Separate the name and value components.
	     */
	    if(valuePtr != NULL) {
	        *valuePtr = '\0';
	        valuePtr++;
	    } else {
	        fprintf(errorLogFd, "AppClass: invalid values for -initial-env\n");
		goto APP_CLASS_ERROR;
	    }
	    
	    OS_EnvString(envPtr, namePtr, valuePtr);
	    valuePtr--;
	    *valuePtr = '=';
	    envPtr++;
	    continue;
	} else if((strcmp(argv[i], "-listen-queue-depth") == 0)) {
	    if((i + 1) == argc) {
	        fprintf(errorLogFd, "AppClass: missing value for -listen-queue-depth\n");
		goto APP_CLASS_ERROR;
	    }
	    i++;
	    listenQueueDepth = atoi(argv[i]);
	    if(listenQueueDepth < 1) {
	        fprintf(errorLogFd, "AppClass: invalid value for -listen-queue-depth\n");
		goto APP_CLASS_ERROR;
	    }
	    continue;
	} else {
	    fprintf(errorLogFd, "AppClass: Invalid field %s\n", argv[i]);
	    goto APP_CLASS_ERROR;
	}
    }
    envPtr = envHead;
    if(numProcesses == -1) {
        numProcesses = 1;
    }
    serverInfoPtr = CreateFcgiServerInfo(numProcesses, execPath);
    if(serverInfoPtr == NULL) {
        fprintf(errorLogFd, "AppClass: redefinition of previously defined app class\n");
	if(envPtr != NULL) {
	  OS_EnvironFree(envPtr);
	  free(envPtr);
	}
	return NULL;
    }

    DStringAppend(&serverInfoPtr->execPath, execPath, -1);
    serverInfoPtr->restartOnExit = TRUE;
    serverInfoPtr->restartDelay = restartDelay;
    serverInfoPtr->processPriority = priority;
    serverInfoPtr->procManager = -1;
    if(listenQueueDepth != -1)
        serverInfoPtr->listenQueueDepth = listenQueueDepth;
    serverInfoPtr->envp = envPtr;
    /*
     * Set envPtr to NULL so that if there is an error we don't end up
     * trying to free up the environment structure twice.  This will be
     * freed up by FreeFcgiServerInfo.
     */
    envPtr = NULL;

    /*
     * Create an initial IPC path for the FCGI listener process.  If 
     * affinity option, then we'll create a unique listener path for
     * each fast cgi process being created.
     *
     * NOTE: The listener socket will be saved and kept in the serverInfo
     * and process info structures for process restart purposes.
     */
    if(affinity == FALSE) {
        listenFd = OS_CreateLocalIpcFd(serverInfoPtr->ipcAddr,
				       serverInfoPtr->listenQueueDepth);
	if(listenFd < 0) {
	    fprintf(errorLogFd, "AppClass: could not create local IPC socket\n");
	    goto APP_CLASS_ERROR;
	} else {
	    serverInfoPtr->listenFd = listenFd;
	}
    }
    free(argv[1]);
    free(argv);
    argv = NULL;
    if (StartFastCGIManager(serverInfoPtr) != 0) {
        goto APP_CLASS_ERROR;
    }
    serverInfoPtr->procStartTime = time(NULL);
    return NULL;

APP_CLASS_ERROR:
    if(serverInfoPtr != NULL) {
        FreeFcgiServerInfo(serverInfoPtr);
    }
    if(envPtr != NULL) {
        OS_EnvironFree(envPtr);
	free(envPtr);
    }
    if (argv) {
      free(argv[1]);
      free(argv);
    }
    return "AppClass: Directive Error";
}


void PrepareClientSocket(WS_Request *reqPtr, int *csdIn, int *csdOut)
{
  fflush(reqPtr->out);
  *csdIn = reqPtr->connection_socket;
  *csdOut = fileno(reqPtr->out);
}
void MakeExtraEnvStr(WS_Request *reqPtr)
{
  make_env_str(reqPtr,"SERVER_HOSTNAME",reqPtr->hostInfo->server_hostname);
  make_env_str(reqPtr,"SERVER_ADDR",
	       inet_ntoa(reqPtr->hostInfo->address_info));
}

int SpawnChild(void (*func)(void *), void *data)
{
  pid_t pid;

  pid = fork();
  if (pid == 0) {
    signal (SIGCHLD, SIG_DFL);
    func (data);
    exit (0);
  }
  return pid;
}
void SetErrorLogFd(void *inp, int type)
{
  if (type) {
    per_host *host = (per_host *) inp;
    if (host && host->error_log)
      errorLogFd = host->error_log;
    else
      errorLogFd = stderr;
  }
  else {
    WS_Request *reqPtr = (WS_Request *) inp;
    if (reqPtr && HostInfo(reqPtr) && HostInfo(reqPtr)->error_log)
      errorLogFd = HostInfo(reqPtr)->error_log;
    else
      errorLogFd = stderr;
  }
}

/*
 *----------------------------------------------------------------------
 *
 * WS_Access --
 *
 *	Determine if a user with the specified user and group id
 *	will be able to access the specified file.  This routine depends
 *	on being called with enough permission to stat the file
 *	(e.g. root).
 *
 *	'mode' is the bitwise or of R_OK, W_OK, or X_OK.
 *
 *	This call is similar to the POSIX access() call, with extra
 *	options for specifying the user and group ID to use for 
 *	checking.
 *
 * Results:
 *      -1 if no access or error accessing, 0 otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
#define WS_SET_errno(x) errno = x
int WS_Access(const char *path, int mode)
{
    struct stat statBuf;
    char **names;
    struct group *grp;
    struct passwd *usr;
    uid_t uid;
    gid_t gid;

    uid = geteuid();
    gid = getegid();

    if(stat(path, &statBuf) < 0)
	return -1;

    /*
     * If the user owns this file, check the owner bits.
     */
    if(uid == statBuf.st_uid) {
	WS_SET_errno(EACCES);
	if((mode & R_OK) && !(statBuf.st_mode & S_IRUSR))
	    goto no_access;

	if((mode & W_OK) && !(statBuf.st_mode & S_IWUSR))
	    goto no_access;

	if((mode & X_OK) && !(statBuf.st_mode & S_IXUSR))
	    goto no_access;

	return 0;	
    }

    /*
     * If the user's group owns this file, check the group bits.
     */
    if(gid == statBuf.st_gid) {
	WS_SET_errno(EACCES);
	if((mode & R_OK) && !(statBuf.st_mode & S_IRGRP))
	    goto no_access;

	if((mode & W_OK) && !(statBuf.st_mode & S_IWGRP))
	    goto no_access;

	if((mode & X_OK) && !(statBuf.st_mode & S_IXGRP))
	    goto no_access;

	return 0;	
    }

    /*
     * Get the group information for the file group owner.  If the
     * user is a member of that group, apply the group permissions.
     */
    grp = getgrgid(statBuf.st_gid);
    if(grp == NULL)
	return -1;

    usr = getpwuid(uid);
    if(usr == NULL)
	return -1;

    for(names = grp->gr_mem; *names != NULL; names++) {
	if(!strcmp(*names, usr->pw_name)) {
	    WS_SET_errno(EACCES);
	    if((mode & R_OK) && !(statBuf.st_mode & S_IRGRP))
		goto no_access;

	    if((mode & W_OK) && !(statBuf.st_mode & S_IWGRP))
		goto no_access;

	    if((mode & X_OK) && !(statBuf.st_mode & S_IXGRP))
		goto no_access;

	    return 0;
        }
    }

    /*
     * If no matching user or group information, use 'other'
     * access information.  
     */
    if((mode & R_OK) && !(statBuf.st_mode & S_IROTH))
	goto no_access;

    if((mode & W_OK) && !(statBuf.st_mode & S_IWOTH))
	goto no_access;

    if((mode & X_OK) && !(statBuf.st_mode & S_IXOTH))
	goto no_access;

    return 0;

no_access:
    WS_SET_errno(EACCES);
    return -1;
}

#endif /* FCGI_SUPPORT */
