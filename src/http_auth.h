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
 * http_auth.h,v 1.20 1995/07/25 14:48:05 blong Exp
 *
 ************************************************************************
 *
 */

#ifndef _HTTP_AUTH_H_
#define _HTTP_AUTH_H_

#ifdef DBM_SUPPORT
# ifndef _DBMSUPPORT_H
#  define _DBMSUPPORT_H
#  include <ndbm.h>
# endif /* _DBMSUPPORT_H */
#endif /* DBM_SUPPORT */

/* globals defined in this module */

extern char user[];
extern char groupname[];


/* http_auth */
void check_auth(per_request *reqInfo, security_data *s, char* auth_line);
int get_pw(per_request *reqInfo, char *user, char *pw, security_data* sec);
#ifdef DBM_SUPPORT
int in_group(per_request *reqInfo, char *user, char *group, char* pchGrps, 
	     DBM* db);
#else
int in_group(per_request *reqInfo, char *user, char *group, char* pchGrps);
#endif /* DBM_SUPPORT */
char* init_group(per_request *reqInfo,char* grpfile);
void auth_bong(per_request *reqInfo,char *s,char* auth_name, char* auth_type);



#if defined(KRB4) || defined(KRB5)
typedef struct _krbdata {
    char client_name[MAX_STRING_LEN];
    char client_realm[MAX_STRING_LEN];
    char server_realm[MAX_STRING_LEN];
    int  ver;
} KerberosInfo;

#define KERBEROSV4 0
#define KERBEROSV5 1

int check_krb_restrict(per_request* reqInfo, security_data* sec, KerberosInfo* kdat);
int krb_in_group(KerberosInfo* kdat, char *group, char* pchGrps);
#endif /* KRB4 or KRB5 */

#ifdef KRB4
extern char k4_srvtab[];
int k4_server_auth(char* authline, char* reply,FILE* error_log, 
		   KerberosInfo *kdat);
#endif	/* KRB4 */

#ifdef KRB5
extern char k5_srvtab[];
int k5_server_auth(char* authline, char* reply, KerberosInfo *kdat);
#endif /* KRB5 */


#endif /* _HTTP_AUTH_H_ */
