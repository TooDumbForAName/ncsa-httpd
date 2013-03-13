/************************************************************************
 * FCGI Interface for the NCSA HTTPd Server
 *
 *  Copyright (C) 1995 Open Market, Inc.
 *  All rights reserved.
 *
 *  This file contains proprietary and confidential information and
 *  remains the unpublished property of Open Market, Inc. Use,
 *  disclosure, or reproduction is prohibited except as permitted by
 *  express written license agreement with Open Market, Inc.
 ************************************************************************
 * $Id: fcgi.h,v 1.2 1996/03/25 22:21:30 blong Exp $
 ************************************************************************
 *
 * fcgi.c -- interface to FCGI
 *
 *  Trung Dung
 *  tdung@openmarket.com
 *
 */

#ifndef _FCGI_H
#define _FCGI_H 1

/* External Functions */
int FastCgiHandler(per_request *reqPtr);
char * AppClassCmd(per_host *host, char *arg);
#endif /* _FCGI_H */
