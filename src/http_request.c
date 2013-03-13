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
 * http_request.c,v 1.113 1996/04/05 18:55:02 blong Exp
 *
 ************************************************************************
 *
 * http_request.c: functions to handle the request structure and initial
 *		   request handling including decoding
 * 
 * 03-21-95  Based on NCSA HTTPd 1.3 by Rob McCool
 * 05-17-95  NCSA HTTPd 1.4.1
 * 05-01-95  NCSA HTTPd 1.4.2
 * 11-10-95  NCSA HTTPd 1.5.0
 * 11-14-95  NCSA HTTPd 1.5.0a
 * 
 */


#include "config.h"
#include "portability.h"

#include <stdio.h>
#ifndef NO_STDLIB_H
# include <stdlib.h>
#endif /* NO_STDLIB_H */
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include "constants.h"
#include "allocate.h"		/* for freeString() */
#include "cgi.h"		/* for exec_cgi_script() */
#ifdef FCGI_SUPPORT
# include "fcgi.h"		/* for FastCgiHandler() */
#endif /* FCGI */
#include "env.h"		/* for free_env() */
#include "http_access.h"	/* for reset_security() */
#include "http_alias.h"		/* For translate_name() */
#include "host_config.h"	/* For which_host_conf(), gConfiguration */
#include "http_config.h"	/* For timeout */
#include "http_log.h"		/* For die(),ErrorStat */
#include "http_send.h"		/* for send_node() */
#include "util.h"		/* for strcasecmp(), etc */
#include "http_request.h"

int req_count = 0;
int cgibuf_count = 0;
int sockbuf_count = 0;

per_request *gCurrentRequest;

/* *sigh* Yeah, we still have some globals left.  We could probably
 * move these into the request structure as well . . . its hitting
 * 100k in size, though.
 */
/* If RFC931 identity check is on, the remote user name */
char *remote_logname;

/* Per request information */
char the_request[HUGE_STRING_LEN];
char as_requested[HUGE_STRING_LEN];
char failed_request[HUGE_STRING_LEN];
char failed_url[HUGE_STRING_LEN];
#ifdef LOG_DURATION
time_t request_time = 0;
#endif /* LOG_DURATION */

/* String constants for the request.  Numbers are in constants.h */
char *methods[METHODS] = {"GET","HEAD","POST","PUT","DELETE","SECURE",
			  "LINK","UNLINK"};
char *protocals[PROTOCALS] = {"HTTP","HTTP/0.9","HTTP/1.0","HTTP/1.1",
			      "Secure-HTTP/1.1", "Secure-HTTP/1.2"};

/* initialize_request() 
 *   either creates, or inits a request structure passed to it.  
 *   This assumes that a request structure passed to it was passed 
 *   through free_request, so it doesn't have any extra pointers that 
 *   need to be dealt with (though it still checks env, that shouldn't
 *   be necessary)
 */
per_request *initialize_request(per_request *reqInfo) 
{
    int RealInit = 0;
    per_request *newInfo;

    RealInit = (reqInfo == NULL) ? 1 : 0;

 
    if (RealInit) {
        newInfo = (per_request *) malloc(sizeof(per_request));
	req_count++;
	reqInfo = newInfo;
	reqInfo->ownDNS = TRUE;
	reqInfo->dns_host_lookup = FALSE;
	reqInfo->remote_name = NULL;
	reqInfo->remote_ip = NULL;
	reqInfo->remote_host = NULL;
	reqInfo->next = NULL;
	reqInfo->ownENV = TRUE;
	reqInfo->env = NULL;
	reqInfo->outh_cgi = NULL;
	reqInfo->RequestFlags = 0;
	reqInfo->sb = NULL;
	reqInfo->ownSB = TRUE;
        reqInfo->cgi_buf = NULL;
    } 

    reqInfo->hostInfo = gConfiguration;

    /* Can't think (now) of any case where environment should transfer
       from last request during KeepAlive, but in other cases, perhaps */

    if (reqInfo->ownENV && reqInfo->env) {
      free_env(reqInfo);
    }

    reqInfo->ownENV = TRUE;
    reqInfo->env = NULL;
    reqInfo->env_len = NULL;
    reqInfo->num_env = 0;
    reqInfo->max_env = 0;


    /* initialize auth stuff */
    reqInfo->bNotifyDomainRestricted = 0;
    reqInfo->bSatisfiedDomain = 0;
    reqInfo->bSatisfiedReferer = 0;

    reqInfo->auth_name = "ByPassword";
    reqInfo->auth_pwfile = NULL;
    reqInfo->auth_pwfile_type = 0;
    reqInfo->auth_grpfile = NULL;
    reqInfo->auth_grpfile_type = 0;
#ifdef DIGEST_AUTH
    reqInfo->auth_digestfile = NULL;
    reqInfo->auth_digestfile_type = 0;
#endif /* DIGEST_AUTH */

   /* eventually, we'll figure out how to let send_fd_timed_out know
      which request.  Until then, keep a global pointer to list */

    gCurrentRequest = reqInfo;

   /* reset security information to access config defaults */
    reset_security();

   /* Initialize Error codes */ 
    ErrorStat = 0;
    reqInfo->status = SC_DOCUMENT_FOLLOWS;
    reqInfo->status_line = NULL; 
    reqInfo->bytes_sent = -1;

    reqInfo->auth_type[0] = '\0';

    reqInfo->url[0] = '\0';
    reqInfo->args[0] = '\0';
    reqInfo->path_info[0] = '\0';
    reqInfo->filename[0] = '\0';

    reqInfo->inh_agent[0] = '\0';
    reqInfo->inh_referer[0] = '\0';
    reqInfo->inh_called_hostname[0] = '\0';
    reqInfo->inh_if_mod_since[0] = '\0';
    reqInfo->inh_auth_line[0] = '\0';
    reqInfo->inh_content_type[0] = '\0';
    reqInfo->inh_content_length = -1;

    reqInfo->outh_location[0] = '\0';
    reqInfo->outh_last_mod[0] = '\0';
    reqInfo->outh_www_auth[0] = '\0';
    reqInfo->outh_content_type[0] = '\0';
    reqInfo->outh_content_encoding[0] = '\0';
    reqInfo->outh_content_length = -1;
#ifdef CONTENT_MD5
    reqInfo->outh_content_md5 = NULL;
#endif /* CONTENT_MD5 */
    reqInfo->outh_cgi = NULL;

    as_requested[0] = '\0';
    failed_url[0] = '\0';
    failed_request[0] = '\0';
    local_default_type[0] = '\0';
    local_default_icon[0] = '\0';

    /* reset keep-alive, client will indicate desire on next request */
    keep_alive.bKeepAlive = 0;
    
    return reqInfo;
}

/* continue_request()
 *   Used (especially in SSI and ErrorDocs) to handle "multiple"
 *   internal requests.  Will probably be used for SHTTP requests
 *   since they can be wrapped multiple times.
 *   Will copy over all inbound http headers, clear all outbound
 *   http headers except outh_www_auth which depends on KEEP_AUTH
 *   Option values in constants.h
 *   Takes as options: 
 *     KEEP_ENV  : Copy ENV from passed request
 *     KEEP_AUTH : Copy auth info from passed request
 *     FORCE_GET : Unless a HEAD request, change to GET request 
 *                 (don't POST to ErrorDoc)
 *     COPY_URL  : Copy URL from passed request
 *     NEW_SOCK_BUF: Set sb to NULL to cause a new sock buf to be created
 */
per_request *continue_request(per_request *reqInfo, int options) {
    per_request *newInfo;

    newInfo = (per_request *)malloc(sizeof(per_request));
    req_count++;
    newInfo->status = reqInfo->status;
    newInfo->status_line = NULL;

    if (options & KEEP_ENV) {
	newInfo->ownENV = FALSE;
	newInfo->env = reqInfo->env;
	newInfo->env_len = reqInfo->env_len;
	newInfo->num_env = reqInfo->num_env;
	newInfo->max_env = reqInfo->max_env;
    } else {
        newInfo->ownENV = TRUE;
        newInfo->env = NULL;
        newInfo->env_len = NULL;
        newInfo->num_env = 0;
        newInfo->max_env = 0;
    }

    if (options & KEEP_AUTH) {
	strcpy(newInfo->auth_type,reqInfo->auth_type);
	strcpy(newInfo->auth_user,reqInfo->auth_user);
	strcpy(newInfo->auth_group,reqInfo->auth_group);
	strcpy(newInfo->inh_auth_line,reqInfo->inh_auth_line);
	strcpy(newInfo->outh_www_auth,reqInfo->outh_www_auth);
	newInfo->auth_name = reqInfo->auth_name;
	newInfo->auth_pwfile = reqInfo->auth_pwfile;
	newInfo->auth_grpfile = reqInfo->auth_grpfile;
	newInfo->auth_pwfile_type = reqInfo->auth_pwfile_type;
	newInfo->auth_grpfile_type = reqInfo->auth_grpfile_type;
#ifdef DIGEST_AUTH
	newInfo->auth_digestfile = reqInfo->auth_digestfile;
	newInfo->auth_digestfile_type = newInfo->auth_digestfile_type;
#endif /* DIGEST_AUTH */
	newInfo->bSatisfiedDomain = reqInfo->bSatisfiedDomain;
	newInfo->bSatisfiedReferer = reqInfo->bSatisfiedReferer;
    } else {
        newInfo->auth_type[0] = '\0';
	newInfo->auth_user[0] = '\0';
	newInfo->auth_group[0] = '\0';
	newInfo->inh_auth_line[0] = '\0';
	newInfo->outh_www_auth[0] = '\0';
        newInfo->auth_name = NULL;
        newInfo->auth_pwfile = NULL;
        newInfo->auth_grpfile = NULL;
        newInfo->auth_pwfile_type = 0;
        newInfo->auth_grpfile_type = 0;
#ifdef DIGEST_AUTH
        newInfo->auth_digestfile = NULL;
        newInfo->auth_digestfile_type = 0;
#endif /* DIGEST_AUTH */
        newInfo->bSatisfiedDomain = FALSE;
	newInfo->bSatisfiedReferer = FALSE;
    }
    newInfo->bNotifyDomainRestricted = reqInfo->bNotifyDomainRestricted;	
    newInfo->bytes_sent = 0;

    if (options & FORCE_GET) {
	if (reqInfo->method != M_HEAD) newInfo->method = M_GET;
	else newInfo->method = M_HEAD;
    } else {
	newInfo->method = reqInfo->method;
    }
    
    newInfo->http_version = reqInfo->http_version;

    newInfo->ownDNS = FALSE;
    newInfo->dns_host_lookup = reqInfo->dns_host_lookup;
    newInfo->remote_host = reqInfo->remote_host;
    newInfo->remote_name = reqInfo->remote_name;
    newInfo->remote_ip = reqInfo->remote_ip;
    newInfo->hostInfo = reqInfo->hostInfo;


    if (options & COPY_URL) {
	strcpy(newInfo->url, reqInfo->url);
	strcpy(newInfo->args, reqInfo->args);
	strcpy(newInfo->path_info, reqInfo->path_info);
	strcpy(newInfo->filename, reqInfo->filename);
    } else {
	newInfo->url[0] = '\0';
	newInfo->args[0] = '\0';
	newInfo->path_info[0] = '\0';
	newInfo->filename[0] = '\0';
    }

    /* Copy all in headers */
    strcpy(newInfo->inh_agent,reqInfo->inh_agent);
    strcpy(newInfo->inh_referer,reqInfo->inh_referer);
    strcpy(newInfo->inh_called_hostname,reqInfo->inh_called_hostname);
    strcpy(newInfo->inh_if_mod_since,reqInfo->inh_if_mod_since);
    strcpy(newInfo->inh_content_type,reqInfo->inh_content_type);
    newInfo->inh_content_length = reqInfo->inh_content_length;

    /* Zero all out headers : outh_www_auth is handled in auth above */
    newInfo->outh_location[0] = '\0';
    newInfo->outh_last_mod[0] = '\0';
    newInfo->outh_content_type[0] = '\0';
    newInfo->outh_content_encoding[0] = '\0';
    newInfo->outh_content_length = -1;
    newInfo->outh_cgi = NULL;

    newInfo->connection_socket = reqInfo->connection_socket;
    newInfo->in = reqInfo->in;
    newInfo->out = reqInfo->out;
    if (options & NEW_SOCK_BUF) {
      newInfo->ownSB = TRUE;
      newInfo->sb = NULL;
    } else {
      newInfo->ownSB = FALSE;
      newInfo->sb = reqInfo->sb;
    }
    newInfo->cgi_buf = NULL;

    newInfo->next = reqInfo;

    gCurrentRequest = newInfo;
    return newInfo;
}

/* free_request()
 *   Clears the request structure linked list.  
 *   Options: (from constants.h)
 *     ONLY_LAST : deletes only the top most (most recent)
 *     NOT_LAST  : Don't delete the last one (so we don't have to malloc again)
 */
void free_request(per_request *reqInfo,int options) {
    per_request *tmp = reqInfo;

    while (reqInfo != NULL) {
	/* We'll just let the freeAllStrings clean up these instead. */
	reqInfo->remote_name = NULL;
	reqInfo->remote_host = NULL;
	reqInfo->remote_ip = NULL;
#ifdef CONTENT_MD5
	if (reqInfo->outh_content_md5 != NULL) free(reqInfo->outh_content_md5);
#endif /* CONTENT_MD5 */
	if (reqInfo->outh_cgi != NULL) free(reqInfo->outh_cgi);

	
	if (reqInfo->ownENV && reqInfo->env) {
	  free_env(reqInfo); 
	}
	if (reqInfo->ownSB && reqInfo->sb) {
	  free(reqInfo->sb);
	  reqInfo->sb = NULL;
	  sockbuf_count--;
	} 
	if (reqInfo->cgi_buf) {
	  free(reqInfo->cgi_buf);
	  reqInfo->cgi_buf = NULL;
	  cgibuf_count--;
	} 
	/* I have no idea how this is possible, but it is happening */
	if (reqInfo == reqInfo->next) reqInfo->next = NULL;
	/* Don't clear the last one */
	if ((reqInfo->next == NULL) && (options & NOT_LAST)) {
          reqInfo->ownDNS = TRUE;
          reqInfo->dns_host_lookup = FALSE;
	  return;
        }
	tmp = reqInfo->next;
	free(reqInfo);
	req_count--;
	reqInfo = tmp;
	gCurrentRequest = reqInfo;	
	if (options & ONLY_LAST) return;
    }
}
	     
/* decode_request()
 *   given an HTTP request line as a string, decodes it into
 *   METHOD URL?ARGS PROTOCAL
 *   Then calls get_http_headers() to get the rfc822 style headers
 */
void decode_request(per_request *reqInfo, char *request) 
{
    char *protocal;
    char *method, *url;
    char *chp;

    /* extract the method */
    method = strtok (request, "\t ");
    if (method) {
	if ((reqInfo->method = MapMethod (method)) == M_INVALID)
	    die(reqInfo,SC_BAD_REQUEST,"Invalid or unsupported method.");
    }
    else
	die(reqInfo,SC_BAD_REQUEST,"Invalid or unsupported method.");
    
    /* extract the URL, and args if present */
    url = strtok (NULL, "\t\r ");
    if (!url) die(reqInfo,SC_BAD_REQUEST,"Incomplete request.");
    if (url && (chp = strchr (url, '?'))) {
	*chp++ = '\0';
	strcpy (reqInfo->args, chp);
    }
    strcpy (reqInfo->url, url);

    protocal = strtok (NULL, "\r");

    if(!protocal) {
        reqInfo->http_version = P_HTTP_0_9;
    }
    else {
	/* On an HTTP/1.0 or HTTP/1.1 request, respond with 1.0 */
        if (!strcmp(protocal,protocals[P_HTTP_1_0])) 
	    reqInfo->http_version = P_HTTP_1_0;
	else if (!strcmp(protocal,protocals[P_HTTP_1_1])) 
	    reqInfo->http_version = P_HTTP_1_0;
        else if (!strcasecmp(protocal,protocals[P_SHTTP_1_1]))
	    reqInfo->http_version = P_SHTTP_1_1;
        else if (!strcasecmp(protocal,protocals[P_SHTTP_1_2]))
	    reqInfo->http_version = P_SHTTP_1_2;
	else reqInfo->http_version = P_OTHER;

	/* dummy call to eat LF at end of protocal */
	strtok (NULL, "\n");
        get_http_headers(reqInfo);
    }

/*    fprintf(stderr,"method:%s url:%s args:%s prot:%s\n",method,
	    reqInfo->url,reqInfo->args, protocal); */
} 


/* Returns a method number for a given method string.  SHTTP requires
 * case independent (as per standard network protocal).  HTTP is broken,
 * and is case dependent.
 */
int MapMethod (char* method)
{
    if(!strcmp(method,methods[M_HEAD]))
	return M_HEAD;
    else if(!strcmp(method,methods[M_GET]))
	return M_GET;
    else if(!strcmp(method,methods[M_POST]))
	return M_POST;
    else if(!strcmp(method,methods[M_PUT]))
	return M_PUT;
    else if(!strcmp(method,methods[M_DELETE]))
	return M_DELETE;
    else if(!strcasecmp(method,methods[M_SECURE]))
	return M_SECURE;
    else 
	return M_INVALID;
}

/* Globals for speed because of size, these are for get_http_headers */
static char field_type[HUGE_STRING_LEN];
static char unrec_hdr[HUGE_STRING_LEN];
static char unrec_hdr_val[HUGE_STRING_LEN];

/* get_http_headers() (formerly get_mime_headers)
 *   Read line by line off from the client, and if we recognize a header,
 *   add it to reqInfo, otherwise let it go through to become an 
 *   environment variable for CGI scripts.  Also, SHTTP headers all start
 *   with SHTTP, so we pass them through to the SHTTP library.
 *   The Content-Type, Content-Length and Authorization headers don't get 
 *   made into CGI variables (Content-Type, Content-Length are already 
 *    defined in the CGI spec, and Authorization would be a security risk.)
 */
void get_http_headers(per_request *reqInfo) 
{
    char *field_val;
    int options = 0;

    while(getline(reqInfo->sb,field_type,HUGE_STRING_LEN-1,options,
		  timeout) != -1) {

        if(!field_type[0]) 
            return;

        if(!(field_val = strchr(field_type,':')))
            continue; 

        *field_val++ = '\0';
        while(isspace(*field_val)) ++field_val;

        if(!strcasecmp(field_type,"Content-type")) {
            strncpy(reqInfo->inh_content_type,field_val,MAX_STRING_LEN);
	    reqInfo->inh_content_type[MAX_STRING_LEN-1] = '\0';
            continue;
        } else 
        if(!strcasecmp(field_type,"Authorization")) {
            strncpy(reqInfo->inh_auth_line,field_val,HUGE_STRING_LEN);
	    reqInfo->inh_auth_line[HUGE_STRING_LEN-1]='\0';
            continue;
        } else 
	if(!strcasecmp(field_type,"Host")) {
	    strncpy(reqInfo->inh_called_hostname, field_val, MAX_STRING_LEN);
	    reqInfo->inh_called_hostname[MAX_STRING_LEN-1] = '\0';
	} else
        if(!strcasecmp(field_type,"Extension")) {
	    if (!strcasecmp(field_val, "Notify-Domain-Restriction"))
		reqInfo->bNotifyDomainRestricted = 1;
#ifdef DIGEST_AUTH
/* Should we do something if we get this header?  Maybe set a flag
 * to know that a client doesn't accept this type of authentication?
 */
/*	    else if (!strcasecmp(field_val, "Security/Digest")); */
#endif /* DIGEST_AUTH */
        } else
        if(!strcasecmp(field_type,"Content-length")) {
            sscanf(field_val,"%d",&(reqInfo->inh_content_length));
            continue;
        } else
        if(!strcasecmp(field_type,"Connection")) {
	    if (!strcasecmp(field_val, "Keep-Alive") && 
		keep_alive.bAllowKeepAlive)
		keep_alive.bKeepAlive = 1;
	} else
        if(!strcasecmp(field_type,"User-agent")) {
	    strncpy(reqInfo->inh_agent, field_val, HUGE_STRING_LEN);
	    reqInfo->inh_agent[HUGE_STRING_LEN-1] = '\0';
        } else
        if(!strcasecmp(field_type,"Referer")) {
	    strncpy(reqInfo->inh_referer, field_val, HUGE_STRING_LEN);
	    reqInfo->inh_referer[HUGE_STRING_LEN-1] = '\0';
        } else 
        if(!strcasecmp(field_type,"If-modified-since")) {
            strncpy(reqInfo->inh_if_mod_since,field_val, MAX_STRING_LEN);
	    reqInfo->inh_if_mod_since[MAX_STRING_LEN-1] = '\0';
	} 
        http2cgi(unrec_hdr, field_type);
	strcpy (unrec_hdr_val, field_val);
        if(reqInfo->env) {
	    if(!merge_header(reqInfo,unrec_hdr,field_val)) 
		make_env_str(reqInfo,unrec_hdr,field_val);
        } 
	else 
	    make_env_str(reqInfo,unrec_hdr,field_val); 
    }
}

/* Split from get_request (the former process_request) so that we can call 
   this from other places (on LOCAL_REDIRECTs and ErrorDocument handling) */

void process_request(per_request *reqInfo) 
{
    int s;

    s = translate_name(reqInfo,reqInfo->url,reqInfo->filename);

    switch(s) {
      case A_STD_DOCUMENT:
	if ((reqInfo->method == M_HEAD) && 
	    (reqInfo->http_version == P_HTTP_0_9)) {
	    reqInfo->method = M_GET;
	    die(reqInfo,SC_BAD_REQUEST,"Invalid HTTP/0.9 method.");
	}
	send_node(reqInfo);
	break;
      case A_REDIRECT_TEMP:
	die(reqInfo,SC_REDIRECT_TEMP,reqInfo->filename);
	break;
      case A_REDIRECT_PERM:
	die(reqInfo,SC_REDIRECT_PERM,reqInfo->filename);
	break;
      case A_SCRIPT_CGI:
	exec_cgi_script(reqInfo);
	break;
#ifdef FCGI_SUPPORT
      case A_SCRIPT_FCGI:
	FastCgiHandler(reqInfo);
	break;
#endif /* FCGI_SUPPORT */
    }
}

/* RequestMain()
 *   The start of the request cycle.  This routine starts reading off
 *   the socket, and also calls the messages for decoding (decode_request()), 
 *   figuring out which virtual host (which_host_conf()), and getting the
 *   DNS info (get_remote_host()).  It then calls process_request() to
 *   handle things once it has all of the information from the client
 *   (with the exception of an entity body).  These are split so that
 *   process_request() can be called internally in the server to handle
 *   new request types (internal redirect handling and ErrorDoc)
 */
void RequestMain(per_request *reqInfo) 
{
   int options = 0;

    signal(SIGPIPE,send_fd_timed_out);

#ifdef LOG_DURATION
    request_time = 0;
#endif /* LOG_DURATION */

    if (reqInfo->sb == NULL) {
      reqInfo->sb = new_sock_buf(reqInfo,reqInfo->in);
      sockbuf_count++;
    }

    if (getline(reqInfo->sb, as_requested, HUGE_STRING_LEN,
		options, timeout) == -1)
        return;

    if(!as_requested[0]) 
        return;

#ifdef LOG_DURATION
       request_time = time(NULL);
#endif /* LOG_DURATION */

    strcpy(the_request, as_requested);

#ifdef SETPROCTITLE
    setproctitle(the_request); 
#endif /* SETPROCTITLE */
    decode_request(reqInfo, as_requested);
    unescape_url(reqInfo->url);

    /* Moved this to later so we can read the headers first for HTTP/1.1
     * Host: support 
     */
    which_host_conf(reqInfo);

    if (reqInfo->dns_host_lookup == FALSE) {
      /* Only when we haven't done DNS do we call get_remote_host().
       * If we aren't supposed to, get_remote_host() will not do it.
       */
      get_remote_host(reqInfo);
    }

    process_request(reqInfo);

}

