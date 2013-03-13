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
 * http_ipc.h,v 1.3 1995/07/25 06:43:36 blong Exp
 *
 ************************************************************************
 *
 */


#ifndef _HTTP_IPC_H_
#define _HTTP_IPC_H_

/* http_ipc function prototypes */
int pass_fd(int spipefd, int filedes);
int recv_fd(int spipefd);

#endif /* _HTTP_IPC_H_ */
