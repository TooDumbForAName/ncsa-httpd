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
 * http_log.h,v 1.15 1995/11/28 09:02:05 blong Exp
 *
 ************************************************************************
 *
 *
 *  http_log.h	contains information for http_log.c including
 *		SERVER_SUPPORT
 *		NUM_ERRORS
 *		struct _ErrorMessage -> ErrorMessage
 *		
 */


#ifndef _HTTP_LOG_H_
#define _HTTP_LOG_H_

/* constants used in this module */

#define LOG_NONE	0
#define LOG_COMBINED    1
#define LOG_SEPARATE    ~(LOG_COMBINED)
#define LOG_SERVERNAME  2
#define LOG_DATE	4

#define SERVER_SUPPORT "httpd@ncsa.uiuc.edu"

/* For Document Error Handling */
#define NUM_DOC_ERRORS      10

/* globals defined in this module */
extern const char StatLine200[];
extern const char StatLine301[];
extern const char StatLine302[];
extern const char StatLine304[];
extern const char StatLine400[];
extern const char StatLine401[];
extern const char StatLine403[];
extern const char StatLine404[];
extern const char StatLine408[];
extern const char StatLine500[];
extern const char StatLine501[];
extern char error_msg[];

extern int ErrorStat;


/* http_log function prototypes */
void log_pid(void);
void log_error(char *err, FILE *fp);
void log_reason(per_request *reqInfo, char *reason, char *file);
void log_transaction(per_request *reqInfo);
void open_logs(per_host *host);
void close_logs(per_host *host);
void error_log2stderr(FILE *error_log);

void title_html(per_request *reqInfo, char *msg);
void begin_http_header(per_request *reqInfo, const char *msg);

int die(per_request *reqInfo, int type, char *err_string);

int GoErrorDoc(per_request *reqInfo, int x, char *ErrString);
int add_doc_error(per_host *host, char* errornum, char* name);
void free_doc_errors(per_host *host);
int have_doc_error(per_request *reqInfo, int errornum);

#endif /* _HTTP_LOG_H_ */

