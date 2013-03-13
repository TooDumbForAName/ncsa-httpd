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
 * cgi.h,v 1.6 1995/11/28 09:01:39 blong Exp
 *
 ************************************************************************
 *
 *
 */


#ifndef _HTTP_SCRIPT_H_
#define _HTTP_SCRIPT_H_

/* function prototypes */
void exec_cgi_script(per_request *reqInfo);
int cgi_stub(per_request *reqInfo, char *path_args, struct stat *finfo);
int add_common_vars(per_request *reqInfo);
void get_path_info(per_request *reqInfo, char *path_args, struct stat *finfo);
int scan_script_header(per_request *reqInfo, int pd);
long send_fd(per_request *reqInfo, int pd, void (*onexit)(void));
long send_nph_script(per_request *reqInfo, int pd, void (*onexit)(void));
void send_cgi(per_request *reqInfo,struct stat *finfo, char *path_args,
	      char allow_options);


void send_fd_timed_out(int);

#endif /* _HTTP_SCRIPT_H_ */







