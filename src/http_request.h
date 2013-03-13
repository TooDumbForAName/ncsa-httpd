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
 * http_request.h,v 1.14 1995/11/09 01:48:21 blong Exp
 *
 ************************************************************************
 *
 *
 */


#ifndef _HTTP_REQUEST_H_
#define _HTTP_REQUEST_H_


/* globals defined in this module */

extern per_request *gCurrentRequest;
extern int no_headers;
extern char *remote_logname;
extern char failed_request[];
extern char failed_url[];
extern int  header_only;
extern char the_request[];

/* Continue Request Options */
#define NEW_URL		1
#define NEW_DNS		2
#define FORCE_GET	4
#define ONLY_LAST	8
#define KEEP_ENV	16
#define KEEP_AUTH	32

/* function prototypes */
per_request *initialize_request(per_request *reqInfo);
per_request *continue_request(per_request *reqInfo, int options);
void free_request(per_request *reqInfo, int options);
void process_request(per_request *reqInfo);
void get_request(per_request *reqInfo);
int MapMethod (char* method);
#endif /* _HTTP_REQUEST_H_ */
