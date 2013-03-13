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
 * http_mime.h,v 1.16 1995/11/28 09:02:08 blong Exp
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

extern char content_type[];
extern char content_type_in[];
extern char content_encoding[];
extern int content_length;
extern char location[];
extern char auth_line[];
extern char called_hostname[];
extern struct mime_ext *Saved_Forced;
extern struct mime_ext *Saved_Encoding;
extern struct mime_ext *forced_types;
extern struct mime_ext *encoding_types;
extern char *out_headers;

/* http_mime function prototypes */
void get_mime_headers(per_request *reqInfo);

void send_http_header(per_request *reqInfo);
void set_content_type(per_request *reqInfo, char *fn);
int set_last_modified(per_request *reqInfo, time_t t);
void probe_content_type(per_request *reqInfo, char *fn);
int scan_script_header(per_request *reqInfo, int pd);
void add_type(per_request *reqInfo, char *fn, char *t);
void add_encoding(per_request *reqInfo, char *fn, char *t);
void set_content_length(per_request *reqInfo, int l);
void dump_types(void);
void init_mime(void);
void kill_mime(void);
void reset_mime_vars(void);
int is_content_type(char *type);
void dump_default_header(per_request *reqInfo);
void init_header_vars(per_request *reqInfo);

extern char *status_line;
char* set_stat_line(per_request *reqInfo);

#endif /* _HTTP_MIME_H_ */
