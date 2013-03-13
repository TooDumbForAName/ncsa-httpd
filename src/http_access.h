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
 * http_access.h,v 1.11 1995/11/28 09:01:50 blong Exp
 *
 ************************************************************************
 *
 */

#ifndef _HTTP_ACCESS_H_
#define _HTTP_ACCESS_H_

#include <sys/stat.h>
/* globals defined in this module */

#define FA_DENY 0
#define FA_ALLOW 1

#ifdef DIGEST_AUTH
extern int client_accepts_digest;
extern int assume_digest_support;
#endif /* DIGEST_AUTH */

/* http_access function prototypes */
void evaluate_access(per_request *reqInfo, struct stat *finfo,int *allow, 
                            char *op);
void kill_security(void);
void reset_security(void);

#endif /* _HTTP_ACCESS_H_ */
