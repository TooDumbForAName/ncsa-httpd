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
 * http_request.c,v 1.99 1995/11/06 20:58:09 blong Exp
 *
 ************************************************************************
 *
 * http_request.c: functions to get and process requests
 * 
 * 03-21-95  Based on NCSA HTTPd 1.3 by Rob McCool
 * 
 * 04-19-95 blong
 *      Forgot to remove free(remote_ip) from here, like elsewhere
 *
 * 04-20-95 blong
 *      Added patch "B18" for redirects w/o delay by Robert Hartill
 *
 * 05-01-95 blong
 *      Added patch by Steve Abatangle (sabat@enterprise.DTS.Harris.COM)
 *      to log SIGPIPE and timed out differently in send_fd_timed_out
 */


#include "config.h"
#include "portability.h"

#include <stdio.h>
#ifndef NO_STDLIB_H
# include <stdlib.h>
#endif /* NO_STDLIB_H */
#include <signal.h>
#include <sys/stat.h>
#include <setjmp.h>
#include <errno.h>
#include <string.h>
#include "constants.h"
#include "httpd.h"
#include "http_request.h"
#include "http_send.h"
#include "cgi.h"
#include "http_access.h"
#include "http_mime.h"
#include "http_config.h"
#include "host_config.h"
#include "http_log.h"
#include "http_auth.h"
#include "http_alias.h"
#include "env.h"
#include "util.h"

int no_headers;
int header_only;
per_request *gCurrentRequest;


/* If RFC931 identity check is on, the remote user name */
char *remote_logname;

/* Per request information */

char the_request[HUGE_STRING_LEN];
char as_requested[HUGE_STRING_LEN];
char failed_request[HUGE_STRING_LEN];
char failed_url[HUGE_STRING_LEN];

char *methods[METHODS] = {"GET","HEAD","POST","PUT","DELETE","LINK","UNLINK"};
char *protocals[PROTOCALS] = {"HTTP","HTTP/0.9","HTTP/1.0","HTTP/1.1"};


per_request *initialize_request(per_request *reqInfo) 
{
    int RealInit = 0;
    per_request *newInfo;

    RealInit = (reqInfo == NULL) ? 1 : 0;

    newInfo = (per_request *) malloc(sizeof(per_request));
    newInfo->hostInfo = gConfiguration;
 
    if (RealInit) {
	reqInfo = newInfo;
	reqInfo->ownURL = TRUE;
	reqInfo->ownDNS = TRUE;
	reqInfo->dns_host_lookup = FALSE;
	reqInfo->remote_name = NULL;
	reqInfo->remote_ip = NULL;
	reqInfo->remote_host = NULL;
	reqInfo->next = NULL;
    } else {
	newInfo->next = reqInfo;
	reqInfo = newInfo;
	reqInfo->ownURL = FALSE;
	reqInfo->ownDNS = FALSE;
	reqInfo->dns_host_lookup = reqInfo->next->dns_host_lookup;
	reqInfo->remote_name = reqInfo->next->remote_name;
	reqInfo->remote_host = reqInfo->next->remote_host;
	reqInfo->remote_ip = reqInfo->next->remote_ip;
	reqInfo->hostInfo = reqInfo->next->hostInfo;
    }

    /* Can't think (now) of any case where environment should transfer
       from last request during KeepAlive, but in other cases, perhaps */

    reqInfo->ownENV = TRUE;
    reqInfo->env = NULL;
    reqInfo->env_len = NULL;
    reqInfo->num_env = 0;
    reqInfo->max_env = 0;

    /* initialize auth stuff */
    reqInfo->bNotifyDomainRestricted = 0;
    reqInfo->bSatisfiedDomain = 0;

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
/*    status = 200; */
    reqInfo->status = SC_DOCUMENT_FOLLOWS;
/*    reqInfo->status_line = NULL; */
    reqInfo->bytes_sent = -1;

    reqInfo->auth_type[0] = '\0';

    if (!reqInfo->ownURL) {
	strcpy(reqInfo->url, reqInfo->next->url);
	strcpy(reqInfo->args, reqInfo->next->args);
	strcpy(reqInfo->filename, reqInfo->next->filename);
    }
    reqInfo->url[0] = '\0';
    reqInfo->args[0] = '\0';
    reqInfo->filename[0] = '\0';

    reqInfo->agent[0] = '\0';
    reqInfo->referer[0] = '\0';

    init_header_vars(reqInfo);
    as_requested[0] = '\0';
    failed_url[0] = '\0';
    failed_request[0] = '\0';
    local_default_type[0] = '\0';
    local_default_icon[0] = '\0';

/* All but HEAD send more than a header */
    header_only = 0;

    /* reset keep-alive, client will indicate desire on next request */
    keep_alive.bKeepAlive = 0;
    
    return reqInfo;
}

per_request *continue_request(per_request *reqInfo, int options) {
    per_request *newInfo;

    newInfo = (per_request *)malloc(sizeof(per_request));
    newInfo->status = reqInfo->status;

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
    } else {
        newInfo->auth_type[0] = '\0';
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

    if (options & NEW_DNS) {
	newInfo->ownDNS = TRUE;
	newInfo->dns_host_lookup = FALSE;
	newInfo->remote_host = NULL;
	newInfo->remote_name = NULL;
	newInfo->remote_ip = NULL;
	newInfo->hostInfo = NULL;
    } else {
	newInfo->ownDNS = FALSE;
	newInfo->dns_host_lookup = reqInfo->dns_host_lookup;
	newInfo->remote_host = reqInfo->remote_host;
	newInfo->remote_name = reqInfo->remote_name;
	newInfo->remote_ip = reqInfo->remote_ip;
	newInfo->hostInfo = reqInfo->hostInfo;
    }

    if (options & NEW_URL) {
	newInfo->ownURL = TRUE;
	newInfo->url[0] = '\0';
	newInfo->args[0] = '\0';
	newInfo->filename[0] = '\0';
    } else {
	newInfo->ownURL = FALSE;
	strcpy(newInfo->url, reqInfo->url);
	strcpy(newInfo->args, reqInfo->args);
	strcpy(newInfo->filename, reqInfo->filename);
    }

    strcpy(newInfo->referer,reqInfo->referer);
    strcpy(newInfo->agent,reqInfo->agent);

    newInfo->connection_socket = reqInfo->connection_socket;
    newInfo->out = reqInfo->out;

    newInfo->next = reqInfo;

    gCurrentRequest = newInfo;
    return newInfo;
}

void free_request(per_request *reqInfo,int options) {
    per_request *tmp = reqInfo;

    while (reqInfo != NULL) {
	if (reqInfo->ownDNS) {
	    if (reqInfo->remote_name != NULL) free(reqInfo->remote_name);
	    if (reqInfo->remote_host != NULL) free(reqInfo->remote_host);
	    if (reqInfo->remote_ip != NULL) free(reqInfo->remote_ip);
	}
	
	if (reqInfo->ownENV && reqInfo->env) {
	  free_env(reqInfo); 
	}
	tmp = reqInfo->next;
	free(reqInfo);
	reqInfo = tmp;
	gCurrentRequest = reqInfo;	
	if (options & ONLY_LAST) return;
    }
}
	     
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
    if (url && (chp = strchr (url, '?'))) {
	*chp++ = '\0';
	strcpy (reqInfo->args, chp);
    }
    strcpy (reqInfo->url, url);

    protocal = strtok (NULL, "\r");

    if(!protocal) {
        no_headers = 1;
        reqInfo->http_version = P_HTTP_0_9;
    }
    else {
        no_headers = 0;
        if (!strcmp(protocal,"HTTP/1.0")) 
	    reqInfo->http_version = P_HTTP_1_0;
	else if (!strcmp(protocal,"HTTP/1.1")) 
	    reqInfo->http_version = P_HTTP_1_1;
	else reqInfo->http_version = P_OTHER;

	/* dummy call to eat LF at end of protocal */
	strtok (NULL, "\n");
        get_mime_headers(reqInfo);
    }

/*    fprintf(stderr,"method:%s url:%s args:%s prot:%s\n",method,
	    reqInfo->url,reqInfo->args, protocal); */
} 


void get_request(per_request *reqInfo) 
{

    signal(SIGPIPE,send_fd_timed_out);


    if (getline(reqInfo->connection_socket, as_requested, HUGE_STRING_LEN,
		1, timeout) == -1)
        return;

    if(!as_requested[0]) 
        return;

    strcpy(the_request, as_requested);

#ifdef SETPROCTITLE
    setproctitle(the_request); 
#endif /* SETPROCTITLE */
    decode_request(reqInfo, as_requested);
    unescape_url(reqInfo->url);

    /* Moved this to later so we can read the headers first for HTTP/1.1
       Host: support */
    which_host_conf(reqInfo);
    if (reqInfo->ownDNS) {
      /* Only when ownDNS set do we find out the remote host name info
	 and by which name the server was called */
      get_remote_host(reqInfo);
    }

    if(reqInfo->method == M_HEAD) 
        header_only=1;
    else if(reqInfo->method == M_GET) {
    }

    process_request(reqInfo);

}

int MapMethod (char* method)
{
    if(!strcmp(method,"HEAD"))
	return M_HEAD;
    else if(!strcmp(method,"GET"))
	return M_GET;
    else if(!strcmp(method,"POST"))
	return M_POST;
    else if(!strcmp(method,"PUT"))
	return M_PUT;
    else if(!strcmp(method,"DELETE"))
	return M_DELETE;
    else 
	return M_INVALID;
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
	    header_only = 0;
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
    }
}
