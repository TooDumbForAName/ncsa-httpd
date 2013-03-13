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
 * httpd.h,v 1.91 1995/11/28 09:02:14 blong Exp
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

typedef struct _ChildInfo {
  int parentfd;
  int childfd;
  int pid;
  int busy;
} ChildInfo;

extern KeepAliveData keep_alive;  /* global keep alive info */
extern JMP_BUF jmpbuffer;	  /* Return buffer for siglongjmp */

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
