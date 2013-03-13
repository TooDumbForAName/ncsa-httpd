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
 *  Modified from source provided by: Eric W. Sink, Spyglass
 *
 */

#include "config.h"
#include "portability.h"

#ifdef DIGEST_AUTH

#include <stdio.h>
#ifndef NO_STDLIB_H 
# include <stdlib.h>
#endif /* NO_STDLIB_H */
#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#ifdef DBM_SUPPORT
# ifndef _DBMSUPPORT_H  /* moronic OSs which don't protect their own include */
#  define _DBMSUPPORT_H  /* files from being multiply included */
#  include <ndbm.h>
# endif /* _DBMSUPPORT_H */
#endif /* DBM_SUPPORT */
#include "constants.h"
#include "fdwrap.h"
#include "digest.h"
#include "http_request.h"
#include "http_log.h"
#include "http_auth.h"
#include "http_mime.h"
#include "util.h"

int get_digest(per_request *reqInfo, char *user, char *realm, char *digest, 
	       security_data* sec) 
{
    FILE *f;
    char errstr[MAX_STRING_LEN];
    char l[MAX_STRING_LEN];
    char w[MAX_STRING_LEN];
    char r[MAX_STRING_LEN];

    if (reqInfo->auth_digestfile_type == AUTHFILETYPE_STANDARD) {
	if (reqInfo->auth_digestfile == NULL) {
	    sprintf (errstr, "No digest file specified for URL: %s\n",
	             reqInfo->url);
	    die(reqInfo,SC_SERVER_ERROR,errstr);
	}
	if(!(f=FOpen(reqInfo->auth_digestfile,"r"))) {
	    sprintf(errstr,"Could not open digest file %s",
		    reqInfo->auth_digestfile);
	    die(reqInfo,SC_SERVER_ERROR,errstr);
	}
	while(!(cfg_getline(l,MAX_STRING_LEN,f))) {
	    if((l[0] == '#') || (!l[0])) continue;
	    getword(w,l,':');
	    getword(r,l,':');

	    if ((0 == strcmp(user,w)) && (0 == strcmp(realm,r))) {
		if (strlen(l) == 32) {
		    strcpy(digest,l);
		    FClose(f);
		    return 1;
		}
		else {
		    FClose(f);
		    sprintf(errstr,
			    "In digest file %s, digest for %s:%s must be 32 chars",
			    reqInfo->auth_digestfile,user,realm);
		    die(reqInfo,SC_SERVER_ERROR,errstr);
		}
	    }
	}
	FClose(f);
	return 0;
    }
#ifdef DBM_SUPPORT
    else if (reqInfo->auth_digestfile_type == AUTHFILETYPE_DBM) {
	DBM* db;
	datum dtKey, dtRec;
	char szBuf[2*MAX_STRING_LEN];

	if(!(db = DBM_Open(reqInfo->auth_digestfile, O_RDONLY, 0))) {
	    sprintf(errstr,"Could not open user file %s",reqInfo->auth_digestfile);
	    die(reqInfo,SC_SERVER_ERROR,errstr);
	}
	sprintf (szBuf, "%s:%s", user, realm);
	dtKey.dptr = szBuf;
	dtKey.dsize = strlen(szBuf);
	dtRec = dbm_fetch(db, dtKey);
	DBM_Close(db);
	if (dtRec.dptr) {
	    strncpy(digest, dtRec.dptr, dtRec.dsize);
	    digest[dtRec.dsize] = '\0';
	    return 1;
	}
	else
	    return 0;
    }
#endif /* DBM_SUPPORT */
#ifdef NIS_SUPPORT
    else if (reqInfo->auth_pwfile_type == AUTHFILETYPE_NIS) {
      char    *domain,
              *digest,
              *resptr,
              szBuf[2*MAX_STRING_LEN];
      int     yperr,
              resize;

      if (init_nis(&domain) != 0)
              return 0;

      if (strcmp(reqInfo->auth_digestfile, "+"))
              digest = reqInfo->auth_digestfile;
      else
              digest = "digest";

      (void) sprintf(szBuf, "%s:%s", user, realm);

      yperr = yp_match(domain, digest, szBuf, strlen(szBuf), &resptr, &resize);
      if (yperr == 0) {
              getword(w, resptr, ':');
              getword(r, resptr, ':');
              if (strcmp(w, user) == 0 && strcmp(w, realm) == 0) {
                      getword(w, resptr, ':');
                      (void) strcpy(digest, w);
                      return 1;
              }
      }
      return 0;
    } 
#endif /* NIS_SUPPORT */
    else 
      die(reqInfo,SC_SERVER_ERROR,"Invalid password file type");
    return 0;
}

void Digest_Construct401(per_request *reqInfo, char *s, int stale, char* auth_name)
{
	char timestamp[32];
	char h_opaque[33];
	char opaque[MAX_STRING_LEN];

	/*
		Note that the domain field isn't being sent at all.  If
		it were to be sent, it would probably need to be read
		from the config files.

		We're using timestamps as our nonce value.
	*/

	/*
		Grab the timestamp (for the nonce).  Also, then construct
		the opaque value.
	*/
	sprintf(timestamp, "%d", time(NULL));
	sprintf(opaque, "%s:%s:%s", auth_name, timestamp, reqInfo->remote_ip);
	md5(opaque, h_opaque);

	if (stale)
	{
		sprintf(s, "Digest realm=\"%s\" nonce=\"%s\" opaque=\"%s\" stale=TRUE",
			auth_name, timestamp, h_opaque);
	}
	else
	{
		sprintf(s, "Digest realm=\"%s\" nonce=\"%s\" opaque=\"%s\"",
			auth_name, timestamp, h_opaque);
	}
}

void Digest_Check(per_request *reqInfo, char *user, security_data* sec)
{
    char username[MAX_STRING_LEN];
    char realm[MAX_STRING_LEN];
    char nonce[MAX_STRING_LEN];
    char uri[MAX_STRING_LEN];
    char response[32 + 1];
    char opaque[MAX_STRING_LEN];
    char errstr[MAX_STRING_LEN];

    char *p;
    char *q;
    
    time_t time_now;
    time_t time_nonce;

/*    user[0]; */	/* assume that we won't succeed */

    username[0] = 0;
    realm[0] = 0;
    nonce[0] = 0;
    uri[0] = 0;
    response[0] = 0;
    opaque[0] = 0;
    p = q = NULL;
    
    p = reqInfo->inh_auth_line;
    while (isspace(*p))	{
	p++;
    }
    if (!strncmp(p, reqInfo->auth_name, strlen(reqInfo->auth_name))) {
	Digest_Construct401(reqInfo,errstr,1,reqInfo->auth_name);
	die(reqInfo,SC_AUTH_REQUIRED,errstr);
	return;
    }

    while (!isspace(*p)) {
	p++;
    }
    p++;
    /* p now points to the first keyword-value pair */
    
    while (*p) {
	if (!strncmp(p, "username=", 9)) {
	    /* skip to the quoted value */
	    p = strchr(p, '"');
	    p++;
	    
	    q = username;
	    while (*p != '"') {
		*q++ = *p++;
	    }
	    *q = 0;
	}
	else if (!strncmp(p, "realm=", 6)) {
	    /* skip to the quoted value */
	    p = strchr(p, '"');
	    p++;
	    
	    q = realm;
	    while (*p != '"') {
		*q++ = *p++;
	    }
	    *q = 0;
	}
	else if (!strncmp(p, "nonce=", 6)) {
	    /* skip to the quoted value */
	    p = strchr(p, '"');
	    p++;
	    
	    q = nonce;
	    while (*p != '"') {
		*q++ = *p++;
	    }
	    *q = 0;
	}
	else if (!strncmp(p, "uri=", 4)) {
	    /* skip to the quoted value */
	    p = strchr(p, '"');
	    p++;
	    
	    q = uri;
	    while (*p != '"') {
		*q++ = *p++;
	    }
	    *q = 0;
	}
	else if (!strncmp(p, "response=", 9)) {
	    /* skip to the quoted value */
	    p = strchr(p, '"');
	    p++;
	    
	    q = response;
	    while (*p != '"') {
		*q++ = *p++;
	    }
	    *q = 0;
	}
	else if (!strncmp(p, "opaque=", 7)) {
	    /* skip to the quoted value */
	    p = strchr(p, '"');
	    p++;
	    
	    q = opaque;
	    while (*p && *p != '"') {
		*q++ = *p++;
	    }
	    *q = 0;
	}

	/*
	   Skip to the next keyword value pair, or the end
	   */
	while (*p && (*p != ',')) {
	    p++;
	}
	if (*p == ',') {
	    while (!isalpha(*p)) {
		p++;
	    }
	}
    }

    {
	char h_a1[32 + 1];
	char a2[MAX_STRING_LEN];
	char h_a2[32 + 1];
	char all[MAX_STRING_LEN];
	char h_all[32 + 1];
	
	/*
	   First, check to make sure the nonce is not stale
	   */

	time_nonce = atoi(nonce);
	time_now = time(NULL);
	
	if ((time_nonce > time_now) ||
	    (time_nonce < (time_now - DIGEST_NONCE_WINDOW))) {
	    /* the nonce is stale */
	    
	    Digest_Construct401(reqInfo,errstr,1,reqInfo->auth_name);
	    die(reqInfo,SC_AUTH_REQUIRED,errstr);
	    return;
	}

	/*
	   Check to make sure that the opaque string is valid.
	   */
	{
	    char h_opaque[33];
	    char check_opaque[MAX_STRING_LEN];
	    
	    sprintf(check_opaque, "%s:%s:%s", realm, nonce, 
		    reqInfo->remote_ip);
	    md5(check_opaque, h_opaque);
	    if (0 != strcmp(h_opaque, opaque)) {
		Digest_Construct401(reqInfo,errstr,1,reqInfo->auth_name);
		die(reqInfo,SC_AUTH_REQUIRED,errstr);
		return;
	    }
	}
	
	/*
	   Here we should check to make sure that the URI
	   given is valid, but a simple strcmp may not be reliable.
	   */
#if 0
	if (0 != strcmp(reqInfo->url, uri)) {
	    Digest_Construct401(reqInfo,errstr,1,reqInfo->auth_name);
	    die(reqInfo,SC_AUTH_REQUIRED,errstr);
			return;
	}
#endif /* 0 */
	
	/*
	   Now, check to make sure that the MD5 digest given
	   is correct.
	   */
	
	if (!get_digest(reqInfo,username, realm, h_a1, sec)) {
	    sprintf(errstr,"user:realm %s:%s not found",user,realm);
	    auth_bong(reqInfo,errstr,reqInfo->auth_name,sec->auth_type);
	}
	
	sprintf(a2, "%s:%s", methods[reqInfo->method], uri);
	md5(a2, h_a2);

	sprintf(all, "%s:%s:%s", h_a1, nonce, h_a2);

	md5(all, h_all);
	if (0 == strcmp(h_all, response)) {
	    strcpy(user, username);
	}
	else {
	    Digest_Construct401(reqInfo,errstr,1,reqInfo->auth_name);
	    die(reqInfo,SC_AUTH_REQUIRED,errstr);
	    return;
	}
    }
}

#endif /* DIGEST_AUTH */

