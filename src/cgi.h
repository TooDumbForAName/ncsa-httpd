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
 * cgi.h,v 1.9 1996/04/05 18:54:40 blong Exp
 *
 ************************************************************************
 *
 *
 */


#ifndef _HTTP_SCRIPT_H_
#define _HTTP_SCRIPT_H_

#include <sys/stat.h>

/* function prototypes */
void exec_cgi_script(per_request *reqInfo);
int cgi_stub(per_request *reqInfo, struct stat *finfo, int allow_options);
int add_common_vars(per_request *reqInfo);
int add_cgi_vars(per_request *reqInfo, int *content);
void get_path_info(per_request *reqInfo, struct stat *finfo);
int scan_cgi_header(per_request *reqInfo, int pd);
long send_fd(per_request *reqInfo, int pd, void (*onexit)(void));
long send_nph_script(per_request *reqInfo, int pd, void (*onexit)(void));
void send_cgi(per_request *reqInfo,struct stat *finfo, char allow_options);
void internal_redirect(per_request *reqInfo);

#endif /* _HTTP_SCRIPT_H_ */
