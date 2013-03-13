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
 * http_send.h,v 1.10 1996/03/27 20:44:12 blong Exp
 *
 ************************************************************************
 *
 */


#ifndef _HTTP_SEND_H_
#define _HTTP_SEND_H_

/* function prototypes */
void send_node(per_request *reqInfo);
void send_file(per_request *reqInfo, struct stat *fi, char allow_options);
void send_dir(per_request *reqInfo,struct stat *finfo, char allow_options);
int extract_path_info(per_request *reqInfo, struct stat *finfo);
long send_fp(per_request *reqInfo, FILE *f, void (*onexit)(void));
void send_fd_timed_out(int);
void send_http_header(per_request *reqInfo);

int rprintf(per_request *reqInfo, char *format, ...);
int rputs(char *string, per_request *reqInfo);
int rputc(int ch, per_request *reqInfo);
int rflush(per_request *reqInfo);

#endif /* _HTTP_SEND_H_ */

