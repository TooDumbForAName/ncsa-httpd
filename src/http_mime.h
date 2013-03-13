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
 * http_mime.h,v 1.19 1996/02/22 23:47:04 blong Exp
 *
 ************************************************************************
 *
 */


#ifndef _HTTP_MIME_H_
#define _HTTP_MIME_H_

/* constants used in this module */

#define MAX_HEADERS 200				

struct mime_ext {
    char *ext;
    char *ct;
    struct mime_ext *next;
};

/* globals defined in this module */


extern struct mime_ext *Saved_Forced;
extern struct mime_ext *Saved_Encoding;
extern struct mime_ext *forced_types;
extern struct mime_ext *encoding_types;

/* http_mime function prototypes */

void set_content_type(per_request *reqInfo, char *fn);
void probe_content_type(per_request *reqInfo, char *fn);
void set_content_length(per_request *reqInfo, int l);
void get_content_type(per_request *reqInfo, char *file,
		      char *content_type, char *content_encoding);

int set_last_modified(per_request *reqInfo, time_t t);

void add_type(per_request *reqInfo, char *fn, char *t);
void add_encoding(per_request *reqInfo, char *fn, char *t);
void dump_types(void);
void init_mime(void);
void kill_mime(void);
void reset_mime_vars(void);

char* set_stat_line(per_request *reqInfo);

#endif /* _HTTP_MIME_H_ */
