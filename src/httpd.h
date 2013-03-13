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
 * httpd.h,v 1.97 1996/03/27 20:44:19 blong Exp
 *
 ************************************************************************
 *
 * httpd.h: header for simple (ha! not anymore) http daemon
 *
 *	contains:
 *		struct _ChildInfo -> ChildInfo
 *
 */

#ifndef _HTTPD_H_
#define _HTTPD_H_

#include <setjmp.h>
#include "http_request.h"

typedef struct _ChildInfo {
  int parentfd;
  int childfd;
  int pid;
  int busy;
#ifdef NOT_READY
  int status;
  KeepAliveData keep_alive;		/* Child's keep alive info */
  int csd;				/* Current Socket Descriptor */
  JMP_BUF restart_child;		/* Return buffer for siglongjmp */
  per_request *gCurrentRequest;		/* Current Request of Child */
#endif /* NOT_READY */
} ChildInfo;

#ifndef NOT_READY
extern KeepAliveData keep_alive;  /* global keep alive info */
extern JMP_BUF jmpbuffer;         /* Return buffer for siglongjmp */
extern int csd;			  /* Current Socket Descriptor */
#endif /* NOT_READY */


/* function prototypes */

void set_signals(void);
char *rfc931(struct sockaddr_in *rmt_sin,struct sockaddr_in *our_sin);
char *get_remote_logname(FILE *fd);

int WaitForRequest(int, KeepAliveData*);
void CompleteRequest(per_request *reqInfo,int pipe);
void GetDescriptor(int parent_pipe);
char* GetRemoteLogName(SERVER_SOCK_ADDR *sa_server);
void htexit(per_request *reqInfo, int status, int die_type);

#endif /* _HTTPD_H_ */
