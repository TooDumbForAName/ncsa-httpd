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
 * http_request.h,v 1.20 1996/04/05 18:55:04 blong Exp
 *
 ************************************************************************
 *
 *
 */


#ifndef _HTTP_REQUEST_H_
#define _HTTP_REQUEST_H_
#include <setjmp.h>



/* globals defined in this module */

extern per_request *gCurrentRequest;
extern char *remote_logname;
extern char failed_request[];
extern char failed_url[];
extern char the_request[];

/* Continue Request Options */
#define COPY_URL	1
#define FORCE_GET	2
#define NOT_LAST	4
#define ONLY_LAST	8
#define KEEP_ENV	16
#define KEEP_AUTH	32
#define NEW_SOCK_BUF	64
extern int req_count;
extern int cgibuf_count;
extern int sockbuf_count;


/* function prototypes */
per_request *initialize_request(per_request *reqInfo);
per_request *continue_request(per_request *reqInfo, int options);
void free_request(per_request *reqInfo, int options);
void get_http_headers(per_request *reqInfo);
void process_request(per_request *reqInfo);
void RequestMain(per_request *reqInfo);
int MapMethod (char* method);
#endif /* _HTTP_REQUEST_H_ */
