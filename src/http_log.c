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
 * http_log.c,v 1.76 1995/11/28 09:02:04 blong Exp
 *
 ************************************************************************
 *
 * http_log.c: Dealing with the logs and errors
 * 
 * Based on NCSA HTTPd 1.3 by Rob McCool
 * 
 * 10/28/94 cvarela
 *    Added agent_log and referer_log files.
 *
 * 02/15/95 blong
 *    Added support for configuration defined error messages
 * 
 * 03/19/95 blong
 *    Some modification to status lines for uniformity, and for user
 *    defined error messages to give the correct status.
 *
 * 04/11/95 blong
 *    Changed custom error responses to only send the error string as
 *    arguments to the script
 *
 * 05/08/95 blong
 *    Bowing to pressure, added patch by Paul Phillips (paulp@cerf.net)
 *    to set CLOSE_ON_EXEC flag for log files under #define SECURE_LOGS
 *
 * 06/01/95 blong
 *    Changed die() so that it only logs the transaction on non-ErrorDocument
 *    errors (errors that are handled internally to the server)
 *
 * 09/13/95 mshapiro
 *    Added log directory checking - group/public write permissions
 *
 * 09-28-95 blong
 *    Added fix by Vince Tkac (tkac@oclc.org) to check if there are any
 *    errordocuments defined for the virtual host in the new schema.
 *
 * 09-29-95 blong
 *    Changed error_document fix to one suggested by Tim Adam (tma@osa.com.au)
 */


#include "config.h"
#include "portability.h"

#include <fcntl.h>
#include <stdio.h>
#ifndef NO_STDLIB_H 
# include <stdlib.h>
#endif /* NO_STDLIB_H */
#ifndef NO_MALLOC_H
# ifdef NEED_SYS_MALLOC_H
#  include <sys/malloc.h>
# else
#  include <malloc.h>
# endif /* NEED_SYS_MALLOC_H */
#endif /* NO_MALLOC_H */
#include <sys/stat.h>
#include <string.h>
#include "constants.h"
#include "http_log.h"
#include "http_request.h"
#include "http_config.h"
#include "host_config.h"
#include "http_auth.h"
#include "http_mime.h"
#include "httpd.h"
#include "util.h"
#include "open_logfile.h"


const char StatLine200[] = "200 Document follows";
const char StatLine204[] = "204 No Content";
const char StatLine301[] = "301 Moved Permanently";
const char StatLine302[] = "302 Moved Temporarily";
const char StatLine304[] = "304 Not modified";
const char StatLine400[] = "400 Bad Request";
const char StatLine401[] = "401 Unauthorized";
const char StatLine403[] = "403 Forbidden";
const char StatLine404[] = "404 Not Found";
const char StatLine408[] = "408 Request Timeout";
const char StatLine500[] = "500 Server Error";
const char StatLine501[] = "501 Not Implemented";
char error_msg[MAX_STRING_LEN];

/* Moved to http_request.c */
int ErrorStat=0;

static int xfer_flags = ( O_WRONLY | O_APPEND | O_CREAT );
static mode_t xfer_mode = ( S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );

void open_logs(per_host *host) {
#ifdef SECURE_LOGS
    int flags;
#endif /* SECURE_LOGS */

    if (host->httpd_conf & HC_ERROR_FNAME) {
      if(!(host->error_log = fopen_logfile(host->error_fname,"a"))) {
        fprintf(stderr,"httpd: could not open error log file %s.\n",
                host->error_fname);
        perror("fopen");
        exit(1);
      }
    }
    if (host->httpd_conf & HC_XFER_FNAME) {
      if((host->xfer_log = open_logfile(host->xfer_fname,xfer_flags, xfer_mode)) < 0) {
        fprintf(stderr,"httpd: could not open transfer log file %s.\n",
                host->xfer_fname);
        perror("open");
        exit(1);
      }
#ifdef SECURE_LOGS
      flags = fcntl(host->xfer_log, F_GETFD);
      flags |= FD_CLOEXEC;
      fcntl(host->xfer_log, F_SETFD, flags);
#endif /* SECURE_LOGS */
    }
    if (!(host->log_opts & LOG_COMBINED)) {
      if (host->httpd_conf & HC_AGENT_FNAME) {
	if(!(host->agent_log = fopen_logfile(host->agent_fname,"a"))) {
	  fprintf(stderr,"httpd: could not open agent log file %s.\n",
		  host->agent_fname);
	  perror("fopen");
	  exit(1);
	}
#ifdef SECURE_LOGS
	flags = fcntl(fileno(host->agent_log), F_GETFD);
	flags |= FD_CLOEXEC;
	fcntl(fileno(host->agent_log), F_SETFD, flags);
#endif /* SECURE_LOGS */
      }
      if (host->httpd_conf & HC_REFERER_FNAME) {
	if(!(host->referer_log = fopen_logfile(host->referer_fname,"a"))) {
	  fprintf(stderr,"httpd: could not open referer log file %s.\n",
		  host->referer_fname);
	  perror("fopen");
	  exit(1);
	}    
#ifdef SECURE_LOGS
	flags = fcntl(fileno(host->referer_log), F_GETFD);
	flags |= FD_CLOEXEC;
	fcntl(fileno(host->referer_log), F_SETFD, flags);
#endif /* SECURE_LOGS */
      }
    }
}

void close_logs(per_host *host) 
{
    if (host->httpd_conf & HC_ERROR_FNAME) {
      fflush(host->error_log);
      fclose(host->error_log);
    }
    if (host->httpd_conf & HC_AGENT_FNAME) {
      fflush(host->agent_log);
      fclose(host->agent_log);
    }
    if (host->httpd_conf & HC_REFERER_FNAME) {
      fflush(host->referer_log);
      fclose(host->referer_log);
    }
    if (host->httpd_conf & HC_XFER_FNAME) {
      close(host->xfer_log);
    }
}

void error_log2stderr(FILE *error_log) 
{
    if(fileno(error_log) != STDERR_FILENO)
        dup2(fileno(error_log),STDERR_FILENO);
}

void log_pid(void) 
{
    FILE *pid_file;

    if(!(pid_file = fopen(pid_fname,"w"))) {
        fprintf(stderr,"httpd: could not log pid to file %s\n",pid_fname);
        exit(1);
    }
    fprintf(pid_file,"%d\n",getpid());
    fclose(pid_file);
}

void log_transaction(per_request *reqInfo) 
{
    char str[HUGE_STRING_LEN];
    long timz;
    struct tm *t;
    char tstr[MAX_STRING_LEN],sign;

    t = get_gmtoff(&timz);
    sign = (timz < 0 ? '-' : '+');
    if(timz < 0) 
        timz = -timz;

    strftime(tstr,MAX_STRING_LEN,"%d/%b/%Y:%H:%M:%S",t);
    sprintf(str,"%s %s %s [%s %c%02ld%02d] \"%s\" ",
            reqInfo->remote_name,
            (do_rfc931 ? remote_logname : "-"),
            (user[0] ? user : "-"),
            tstr,
            sign,
            timz/3600,
            timz%3600,
            the_request); 
    if(reqInfo->status != -1)
        sprintf(str,"%s%d ",str,reqInfo->status);
    else
        strcat(str,"- ");

    if(reqInfo->bytes_sent != -1)
        sprintf(str,"%s%ld",str,reqInfo->bytes_sent);
    else
        strcat(str,"-");
    if (reqInfo->hostInfo->log_opts & LOG_SERVERNAME) {
      if (reqInfo->hostInfo->server_hostname)
	sprintf(str,"%s %s",str,reqInfo->hostInfo->server_hostname);
       else
	strcat(str," -");
    }
    if (reqInfo->hostInfo->referer_ignore && reqInfo->referer[0]) {
       char str[MAX_STRING_LEN];
       int bIgnore = 0;

       lim_strcpy(str, reqInfo->hostInfo->referer_ignore, 255);
       if (reqInfo->hostInfo->referer_ignore[0]) {
         char* tok = strtok (str, " ");

         while (tok) {
           if (strstr(reqInfo->referer, tok)) {
             bIgnore = 1;
             break;
           }
           tok = strtok (NULL, " ");
         }
       }
       if (bIgnore) {
	 reqInfo->referer[0] = '\0';
       }
    }
    if (!(reqInfo->hostInfo->log_opts & LOG_COMBINED)) {
      strcat(str,"\n");
      write(reqInfo->hostInfo->xfer_log,str,strlen(str));

      /* log the user agent */
      if (reqInfo->agent[0]) {
        if (reqInfo->hostInfo->log_opts & LOG_DATE)
	 fprintf(reqInfo->hostInfo->agent_log, "[%s] %s\n",tstr, 
		 reqInfo->agent);
	else
	 fprintf(reqInfo->hostInfo->agent_log, "%s\n", reqInfo->agent);
	fflush(reqInfo->hostInfo->agent_log);
      }
      /* log the referer */
      if (reqInfo->referer[0]) {
	if (reqInfo->hostInfo->log_opts & LOG_DATE)
	  fprintf(reqInfo->hostInfo->referer_log, "[%s] %s -> %s\n",tstr,
	  	  reqInfo->referer, reqInfo->url);
	 else 
	  fprintf(reqInfo->hostInfo->referer_log, "%s -> %s\n",
		  reqInfo->referer, reqInfo->url);
	fflush(reqInfo->hostInfo->referer_log);
      }
    } else {
      if (reqInfo->referer[0])
        sprintf(str,"%s \"%s\"",str,reqInfo->referer);
       else
	strcat(str," \"\"");
      if (reqInfo->agent[0]) 
	sprintf(str,"%s \"%s\"\n",str,reqInfo->agent);
       else
	strcat(str," \"\"\n");
      write(reqInfo->hostInfo->xfer_log,str,strlen(str));
    }
}

void log_error(char *err, FILE *fp) {
    fprintf(fp, "[%s] %s\n",get_time(),err);
    fflush(fp);
}

void log_reason(per_request *reqInfo, char *reason, char *file) 
{
    char *buffer;

    buffer = (char *)malloc(strlen(reason)+strlen(reqInfo->referer)+
			    strlen(reqInfo->remote_name)+strlen(file)+50); 
    sprintf(buffer,"httpd: access to %s failed for %s, reason: %s from %s",
            file,reqInfo->remote_name,reason,
	    ( (reqInfo->referer[0] != '\0') ? reqInfo->referer : "-"));
    log_error(buffer,reqInfo->hostInfo->error_log);
    free(buffer);
}

void begin_http_header(per_request *reqInfo, const char *msg) 
{
    fprintf(reqInfo->out,"%s %s%c",SERVER_PROTOCOL,msg,LF);
    dump_default_header(reqInfo);
}

void error_head(per_request *reqInfo, const char *err) 
{
    if(!no_headers) {
        begin_http_header(reqInfo,err);
        fprintf(reqInfo->out,"Content-type: text/html%c%c",LF,LF);
    }
    if(reqInfo->method != M_HEAD) {
        fprintf(reqInfo->out,"<HEAD><TITLE>%s</TITLE></HEAD>%c",err,LF);
        fprintf(reqInfo->out,"<BODY><H1>%s</H1>%c",err,LF);
    }
}

void title_html(per_request *reqInfo, char *msg) 
{
    fprintf(reqInfo->out,"<HEAD><TITLE>%s</TITLE></HEAD>%c",msg,LF);
    fprintf(reqInfo->out,"<BODY><H1>%s</H1>%c",msg,LF);
}

int die(per_request *reqInfo, int type, char *err_string) 
{
    char arguments[MAX_STRING_LEN];
    int RetVal=0;
    int x;
    int die_type;

    /* kill keepalive on errors until we figure out what to do
       such as compute content_length of error messages */
    die_type = DIE_NORMAL;
    keep_alive.bKeepAlive = 0;
    /*die_type = keep_alive.bKeepAlive ? DIE_KEEPALIVE : DIE_NORMAL;*/

    /* For custom error scripts */
    strcpy(failed_request,the_request);
    strcpy(failed_url,reqInfo->url);

    /* For 1.4b4, changed to have a common message for ErrorDocument calls
       We now send only error=err_string (as passed) and the CGI environment
       variable ERROR_STATUS,ERROR_REQUEST,ERROR_URL contain the rest of the
       relevent information */
    /* For 1.4 release, changed ERROR_ to REDIRECT_ */

    switch(type) {
      case SC_NO_CONTENT:
	reqInfo->status = SC_NO_CONTENT;
        begin_http_header(reqInfo,StatLine204);
        fputc(LF,reqInfo->out);
        keep_alive.bKeepAlive = 0;
        header_only = 1;
        RetVal = SC_NO_CONTENT;
        log_transaction(reqInfo);
        break;
      case SC_REDIRECT_TEMP:
        reqInfo->status = SC_REDIRECT_TEMP;
	if (((x=have_doc_error(reqInfo,type)) >= 0) && (!ErrorStat)) {
	    ErrorStat = reqInfo->status;
	    GoErrorDoc(reqInfo,x,err_string);
	} else {
	    keep_alive.bKeepAlive = 0;
	    if(!no_headers) {
		begin_http_header(reqInfo,StatLine302);
		fprintf(reqInfo->out,"Location: %s%c",err_string,LF);
		fprintf(reqInfo->out,"Content-type: text/html%c",LF);
		fputc(LF,reqInfo->out);
	    }
	    if (reqInfo->method != M_HEAD) {
	      title_html(reqInfo,"Document moved");
	      fprintf(reqInfo->out,
		      "This document has moved <A HREF=\"%s\">here</A>.<P>%c",
		    err_string,LF);
              fprintf(reqInfo->out,"</BODY>%c",LF);
	    }
	    log_transaction(reqInfo);
	}
        break;
      case SC_REDIRECT_PERM:
        reqInfo->status = SC_REDIRECT_PERM;
        if (((x=have_doc_error(reqInfo,type)) >= 0) && (!ErrorStat)) {
            ErrorStat = reqInfo->status;
            GoErrorDoc(reqInfo,x,err_string);
        } else {
            keep_alive.bKeepAlive = 0;
            if(!no_headers) {
                begin_http_header(reqInfo,StatLine301);
                fprintf(reqInfo->out,"Location: %s%c",err_string,LF);
                fprintf(reqInfo->out,"Content-type: text/html%c",LF);
                fputc(LF,reqInfo->out);
            }
            if (reqInfo->method != M_HEAD) {
              title_html(reqInfo,"Document moved");
              fprintf(reqInfo->out,"This document has permanently moved ");
              fprintf(reqInfo->out,"<A HREF=\"%s\">here</A>.<P>%c</BODY>%c",
		      err_string,LF,LF);
            }
            log_transaction(reqInfo);
        }
        break;
      case SC_USE_LOCAL_COPY:
        reqInfo->status = SC_USE_LOCAL_COPY;
        begin_http_header(reqInfo,StatLine304);
        fputc(LF,reqInfo->out);
	keep_alive.bKeepAlive = 0;
        header_only = 1;
	RetVal = SC_USE_LOCAL_COPY;
	log_transaction(reqInfo);
        break;
      case SC_AUTH_REQUIRED:
        reqInfo->status = SC_AUTH_REQUIRED;
	if (((x=have_doc_error(reqInfo,type)) >= 0) && (!ErrorStat)) {
	    ErrorStat = reqInfo->status;
	    GoErrorDoc(reqInfo,x,err_string);
	} else {
	    keep_alive.bKeepAlive = 0;
	    if(!no_headers) {
		begin_http_header(reqInfo,StatLine401);
		fprintf(reqInfo->out,"Content-type: text/html%c",LF);
		fprintf(reqInfo->out,"WWW-Authenticate: %s%c%c",
			err_string,LF,LF);
	    }
	    if (reqInfo->method != M_HEAD) {
	      title_html(reqInfo,"Authorization Required");
	      fprintf(reqInfo->out,
		      "Browser not authentication-capable or %c",LF);
	      fprintf(reqInfo->out,"authentication failed.%c",LF);
              fprintf(reqInfo->out,"</BODY>%c",LF);
 	    }
	    log_transaction(reqInfo);
	}
        break;
      case SC_BAD_REQUEST:
        reqInfo->status = SC_BAD_REQUEST;
	if (((x=have_doc_error(reqInfo,type)) >= 0) && (!ErrorStat)) {
	    ErrorStat = reqInfo->status;
	    GoErrorDoc(reqInfo,x,err_string);
	} else {
	    error_head(reqInfo,StatLine400);
	    keep_alive.bKeepAlive = 0;
	    if (!header_only) {
	      fprintf(reqInfo->out,
		      "Your client sent a query that this server could%c",LF);
	      fprintf(reqInfo->out,"not understand.<P>%c",LF);
	      fprintf(reqInfo->out,"Reason: %s<P>%c",err_string,LF);
              fprintf(reqInfo->out,"</BODY>%c",LF);
	    } 
	    log_transaction(reqInfo);
	}
	break;
      case SC_BAD_IMAGEMAP:
        reqInfo->status = SC_BAD_REQUEST;
	if (((x=have_doc_error(reqInfo,type)) >= 0) && (!ErrorStat)) {
	    ErrorStat = reqInfo->status;
	    GoErrorDoc(reqInfo,x,err_string);
	} else {
	    error_head(reqInfo,StatLine400);
	    keep_alive.bKeepAlive = 0;
	    if (!header_only) {
	      fprintf(reqInfo->out,
		      "Server encountered error processing imagemap%c",LF);
	      fprintf(reqInfo->out,"Reason: %s<P>%c",err_string,LF);
              fprintf(reqInfo->out,"</BODY>%c",LF);
	    } 
	    log_transaction(reqInfo);
	}
	break;
      case SC_FORBIDDEN:
        reqInfo->status = SC_FORBIDDEN;
	if (((x=have_doc_error(reqInfo,type)) >= 0) && (!ErrorStat)) {
	    ErrorStat = reqInfo->status;
	    GoErrorDoc(reqInfo,x,err_string);
	} else {
	    error_head(reqInfo,StatLine403);
	    keep_alive.bKeepAlive = 0;
	    if (reqInfo->method != M_HEAD) {
	      fprintf(reqInfo->out,
		      "Your client does not have permission to get URL %s ",
		    err_string);
	      fprintf(reqInfo->out,"from this server.<P>%c",LF);
              fprintf(reqInfo->out,"</BODY>%c",LF);
	    }	
	    log_transaction(reqInfo);
	}
	break;
      case SC_NOT_FOUND:
        reqInfo->status = SC_NOT_FOUND;
	if (((x=have_doc_error(reqInfo,type)) >= 0) && (!ErrorStat)) {
	    ErrorStat = reqInfo->status;
	    GoErrorDoc(reqInfo,x,err_string);
	} else {
	    error_head(reqInfo,StatLine404);
	    keep_alive.bKeepAlive = 0;
	    if (reqInfo->method != M_HEAD) {
	      fprintf(reqInfo->out,
		      "The requested URL %s was not found on this server.%c",
		      err_string,LF);
              fprintf(reqInfo->out,"</BODY>%c",LF);
	    } 
	    log_transaction(reqInfo);
	}
        break;
      case SC_SERVER_ERROR:
        reqInfo->status = SC_SERVER_ERROR;
        die_type = DIE_NORMAL;
	if (((x=have_doc_error(reqInfo,type)) >= 0) && (!ErrorStat)) {
	    ErrorStat = reqInfo->status;
	    log_error(err_string,reqInfo->hostInfo->error_log);
	    GoErrorDoc(reqInfo,x,err_string);
	} else {
	    error_head(reqInfo,StatLine500);
	    keep_alive.bKeepAlive = 0;
	    log_error(err_string,reqInfo->hostInfo->error_log);
	    if (reqInfo->method != M_HEAD) {
	      fprintf(reqInfo->out,"The server encountered an internal error or%c",LF);
	      fprintf(reqInfo->out,"misconfiguration and was unable to complete %c",LF);
	      fprintf(reqInfo->out,"your request.<P>%c",LF);
	      fprintf(reqInfo->out,"Please contact the server administrator,%c",LF);
	      fprintf(reqInfo->out," %s ",reqInfo->hostInfo->server_admin);
	      fprintf(reqInfo->out,"and inform them of the time the error occurred%c",LF);
	      fprintf(reqInfo->out,", and anything you might have done that may%c",LF);
	      fprintf(reqInfo->out,"have caused the error.<P>%c",LF);
	      fprintf(reqInfo->out,"<b>Error:</b> %s%c",err_string,LF);
              fprintf(reqInfo->out,"</BODY>%c",LF);
	    } 
	    log_transaction(reqInfo);
	}
        break;
      case SC_NOT_IMPLEMENTED:
        reqInfo->status = SC_NOT_IMPLEMENTED;
	if (((x=have_doc_error(reqInfo,type)) >= 0) && (!ErrorStat)) {
	    ErrorStat = reqInfo->status;
	    GoErrorDoc(reqInfo,x,err_string);
	} else {
	    error_head(reqInfo,StatLine501);
	    keep_alive.bKeepAlive = 0;
	    if (reqInfo->method != M_HEAD) {
	      fprintf(reqInfo->out,"We are sorry to be unable to perform the method %s",
		    err_string);
	      fprintf(reqInfo->out," at this time or to this document.<P>%c",LF);
              fprintf(reqInfo->out,"</BODY>%c",LF);
	    }
	    log_transaction(reqInfo);
	}
        break;
      case SC_NO_MEMORY:
        log_error("HTTPd: memory exhausted",reqInfo->hostInfo->error_log);
        reqInfo->status = SC_SERVER_ERROR;
        die_type = DIE_NORMAL;
	error_head(reqInfo,StatLine500);
	keep_alive.bKeepAlive = 0;
	if (reqInfo->method != M_HEAD) {
	    fprintf(reqInfo->out,"The server has temporarily run out of resources%c",LF);
	    fprintf(reqInfo->out,"for your request. Please try again at a later time.<P>%c",LF);
	    fprintf(reqInfo->out,"</BODY>%c",LF);
	}
	log_transaction(reqInfo);
      	break;
      case SC_CONF_ERROR:
	  reqInfo->status = SC_SERVER_ERROR;
	  sprintf(arguments,"httpd: configuration error = %s",err_string);
	  log_error(arguments,reqInfo->hostInfo->error_log);
	  error_head(reqInfo,StatLine500);
	  keep_alive.bKeepAlive = 0;
	  if (reqInfo->method != M_HEAD) {
	      fprintf(reqInfo->out,"The server has encountered a misconfiguration.%c",LF);
	      fprintf(reqInfo->out,"The error was %s.%c",err_string,LF);
	      fprintf(reqInfo->out,"</BODY>%c",LF);
	  }
	  log_transaction(reqInfo);
    }
    fflush(reqInfo->out);
    if (!RetVal) 
      htexit(reqInfo,1,die_type);
    return RetVal;
}

int GoErrorDoc(per_request *reqInfo, int x, char *ErrString) {
    per_request *newInfo;

    newInfo = continue_request(reqInfo,NEW_URL | FORCE_GET | KEEP_ENV | KEEP_AUTH);
    strcpy(newInfo->url,reqInfo->hostInfo->doc_errors[x]->DocErrorFile);
    if (ErrString) 
	sprintf(newInfo->args,"error=%s",ErrString);
    process_request(newInfo); 
    free_request(newInfo,ONLY_LAST);
    return TRUE;
}

/* Add error to error table.  Should be called from http_config.c at startup 
 * With new VirtualHost support, need to create the array of values if this
 * is the first call to add_doc_error.
 */
int add_doc_error(per_host *host, char* errornum, char* name) 
{
    char *tmp;

    if (host->num_doc_errors == 0) {
      host->doc_errors = (ErrorDoc **) malloc(sizeof(ErrorDoc*));
    } else {
      host->doc_errors = (ErrorDoc **) realloc(host->doc_errors,
					       (host->num_doc_errors+1) *
						sizeof(ErrorDoc*));
    }
    host->doc_errors[host->num_doc_errors]=(ErrorDoc *)malloc(sizeof(ErrorDoc));

    tmp = (char *) malloc(strlen(name)+1);
    strcpy(tmp,name);
    
    host->doc_errors[host->num_doc_errors]->DocErrorNum = atoi(errornum);
    host->doc_errors[host->num_doc_errors]->DocErrorFile = tmp;

    return ++(host->num_doc_errors);
}

/* Do we have a defined error for errornum? */
int have_doc_error(per_request *reqInfo, int errornum) 
{
    int x=0;
   
    if (!reqInfo->hostInfo) return -1;

    while ((x < reqInfo->hostInfo->num_doc_errors) && 
	   (reqInfo->hostInfo->doc_errors[x]->DocErrorNum != errornum))
    {
	x++;
    }

    if (x < reqInfo->hostInfo->num_doc_errors) return x;
    else return -1;
}

/* Reset Error Types (for SIGHUP-restart) */
void free_doc_errors(per_host *host) 
{
    int x=0;

    for (x = 0 ; x < host->num_doc_errors; x++) {
      free(host->doc_errors[x]->DocErrorFile);
      free(host->doc_errors[x]);
    }

    free(host->doc_errors);
}
