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
 * http_send.h,v 1.6 1995/11/28 09:02:11 blong Exp
 *
 ************************************************************************
 *
 */


#ifndef _HTTP_SEND_H_
#define _HTTP_SEND_H_

/* function prototypes */
void send_node(per_request *reqInfo);
void send_file(per_request *reqInfo, struct stat *fi, 
               char *path_args, char allow_options);
void send_dir(per_request *reqInfo,struct stat *finfo, char *pa, 
	      char allow_options);
int extract_path_info(per_request *reqInfo, char *path_args,
		      struct stat *finfo);
long send_fp(per_request *reqInfo, FILE *f, void (*onexit)(void));
void send_fd_timed_out(int);

#endif /* _HTTP_SEND_H_ */

