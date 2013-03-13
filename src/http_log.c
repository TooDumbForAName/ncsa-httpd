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
 * http_log.c,v 1.84 1996/04/05 18:54:59 blong Exp
 *
 ************************************************************************
 *
 * http_log.c: Dealing with the logs and errors
 * 
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
#include "allocate.h"
#include "http_log.h"
#include "http_request.h"
#include "http_send.h"
#include "http_config.h"
#include "host_config.h"
#include "http_auth.h"
#include "http_mime.h"
#include "httpd.h"
#include "util.h"
#include "open_logfile.h"


const char StatLine200[] = "200 Document follows";
const char StatLine204[] = "204 No Content";
const char StatLine206[] = "206 Partial Content";
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
const char StatLine503[] = "503 Service Unavailable";
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
        fprintf(stderr,"HTTPd: could not open error log file %s.\n",
                host->error_fname);
        perror("fopen");
        exit(1);
      }
    }
    if (host->httpd_conf & HC_XFER_FNAME) {
      if((host->xfer_log = open_logfile(host->xfer_fname,xfer_flags, xfer_mode)) < 0) {
        fprintf(stderr,"HTTPd: could not open transfer log file %s.\n",
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
	  fprintf(stderr,"HTTPd: could not open agent log file %s.\n",
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
	  fprintf(stderr,"HTTPd: could not open referer log file %s.\n",
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
        fprintf(stderr,"HTTPd: could not log pid to file %s\n",pid_fname);
        exit(1);
    }
    fprintf(pid_file,"%d\n",getpid());
    fclose(pid_file);
}

extern int num_children;

void log_transaction(per_request *reqInfo) 
{
    char *str;
    long timz;
    struct tm *t;
    char *tstr,sign;
#ifdef LOG_DURATION
    extern time_t request_time;
    time_t duration = request_time ? (time(NULL) - request_time) : 0;
#endif /* LOG_DURATION */

    str = newString(HUGE_STRING_LEN,STR_TMP);
    tstr = newString(MAX_STRING_LEN,STR_TMP);


    t = get_gmtoff(&timz);
    sign = (timz < 0 ? '-' : '+');
    if(timz < 0) 
        timz = -timz;

    strftime(tstr,MAX_STRING_LEN,"%d/%b/%Y:%H:%M:%S",t);
    sprintf(str,"%s %s %s [%s %c%02ld%02d] \"%s\" ",
            (reqInfo->remote_name ? reqInfo->remote_name : "-"),
            (do_rfc931 ? remote_logname : "-"),
            (reqInfo->auth_user[0] ? reqInfo->auth_user : "-"),
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
    if (reqInfo->hostInfo->referer_ignore && reqInfo->inh_referer[0]) {
       char *str1;
       int bIgnore = 0;
       
       str1 = newString(MAX_STRING_LEN,STR_TMP);
       lim_strcpy(str1, reqInfo->hostInfo->referer_ignore, 255);
       if (reqInfo->hostInfo->referer_ignore[0]) {
         char* tok = strtok (str1, " ");

         while (tok) {
           if (strstr(reqInfo->inh_referer, tok)) {
             bIgnore = 1;
             break;
           }
           tok = strtok (NULL, " ");
         }
       }
       if (bIgnore) {
	 reqInfo->inh_referer[0] = '\0';
       }
       freeString(str1);
    }
#ifdef LOG_DURATION
    sprintf(str+strlen(str), " %ld", duration);
#endif /* LOG_DURATION */

    if (!(reqInfo->hostInfo->log_opts & LOG_COMBINED)) {
      strcat(str,"\n");
      write(reqInfo->hostInfo->xfer_log,str,strlen(str));

      /* log the user agent */
      if (reqInfo->inh_agent[0]) {
        if (reqInfo->hostInfo->log_opts & LOG_DATE)
	 fprintf(reqInfo->hostInfo->agent_log, "[%s] %s\n",tstr, 
		 reqInfo->inh_agent);
	else
	 fprintf(reqInfo->hostInfo->agent_log, "%s\n", reqInfo->inh_agent);
	fflush(reqInfo->hostInfo->agent_log);
      }
      /* log the referer */
      if (reqInfo->inh_referer[0]) {
	if (reqInfo->hostInfo->log_opts & LOG_DATE)
	  fprintf(reqInfo->hostInfo->referer_log, "[%s] %s -> %s\n",tstr,
	  	  reqInfo->inh_referer, reqInfo->url);
	 else 
	  fprintf(reqInfo->hostInfo->referer_log, "%s -> %s\n",
		  reqInfo->inh_referer, reqInfo->url);
	fflush(reqInfo->hostInfo->referer_log);
      }
    } else {
      if (reqInfo->inh_referer[0])
        sprintf(str,"%s \"%s\"",str,reqInfo->inh_referer);
       else
	strcat(str," \"\"");
      if (reqInfo->inh_agent[0]) 
	sprintf(str,"%s \"%s\"\n",str,reqInfo->inh_agent);
       else
	strcat(str," \"\"\n");
      write(reqInfo->hostInfo->xfer_log,str,strlen(str));
    }
    freeString(str);
    freeString(tstr);
}

void log_error(char *err, FILE *fp) {
    fprintf(fp, "[%s] %s\n",get_time(),err);
    fflush(fp);
}

void log_reason(per_request *reqInfo, char *reason, char *file) 
{
    char *buffer;

    /* This might not be big enough, but since its in the heap, the
     * worst that will happen is a core dump, and its faster than a
     * malloc, and won't fragment the memory as badly.
     */
    buffer = newString(HUGE_STRING_LEN,STR_TMP);

    sprintf(buffer,"HTTPd: access to %s failed for %s, reason: %s from %s",
            file,reqInfo->remote_name,reason,
	    ( (reqInfo->inh_referer[0] != '\0') ? reqInfo->inh_referer : "-"));
    log_error(buffer,reqInfo->hostInfo->error_log);
    freeString(buffer);
}


void error_head(per_request *reqInfo, const char *err) 
{
    if(reqInfo->http_version != P_HTTP_0_9) {
      strcpy(reqInfo->outh_content_type,"text/html");
      send_http_header(reqInfo);
    } 
    if(reqInfo->method != M_HEAD) {
        rprintf(reqInfo,"<HEAD><TITLE>%s</TITLE></HEAD>%c",err,LF);
        rprintf(reqInfo,"<BODY><H1>%s</H1>%c",err,LF);
    }
}

void title_html(per_request *reqInfo, char *msg) 
{
    rprintf(reqInfo,"<HEAD><TITLE>%s</TITLE></HEAD>%c",msg,LF);
    rprintf(reqInfo,"<BODY><H1>%s</H1>%c",msg,LF);
}

int die(per_request *reqInfo, int type, char *err_string) 
{
    char *arguments;
    int RetVal=0;
    int x;
    int die_type;

    arguments = newString(MAX_STRING_LEN,STR_TMP);
    /* kill keepalive on errors until we figure out what to do
       such as compute content_length of error messages */
    die_type = DIE_NORMAL;
    keep_alive.bKeepAlive = 0;
    /*die_type = keep_alive.bKeepAlive ? DIE_KEEPALIVE : DIE_NORMAL;*/

    /* For custom error scripts */
    strcpy(failed_request,the_request);
    strcpy(failed_url,reqInfo->url);

    switch(type) {
      case SC_NO_CONTENT:
	reqInfo->status = SC_NO_CONTENT;
	set_stat_line(reqInfo);
	if (reqInfo->http_version != P_HTTP_0_9) {
	  send_http_header(reqInfo);
	}
        keep_alive.bKeepAlive = 0;
        RetVal = SC_NO_CONTENT;
        log_transaction(reqInfo);
        break;
      case SC_REDIRECT_TEMP:
        reqInfo->status = SC_REDIRECT_TEMP;
	set_stat_line(reqInfo);
	if (((x=have_doc_error(reqInfo,type)) >= 0) && (!ErrorStat)) {
	    ErrorStat = reqInfo->status;
	    GoErrorDoc(reqInfo,x,err_string);
	} else {
	    keep_alive.bKeepAlive = 0;
	    if(reqInfo->http_version != P_HTTP_0_9) {
	      strcpy(reqInfo->outh_location,err_string);
	      strcpy(reqInfo->outh_content_type,"text/html");
	      send_http_header(reqInfo);
	    }
	    if (reqInfo->method != M_HEAD) {
	      title_html(reqInfo,"Document moved");
	      rprintf(reqInfo,
		      "This document has moved <A HREF=\"%s\">here</A>.<P>%c",
		    err_string,LF);
              rprintf(reqInfo,"</BODY>%c",LF);
	    }
	    log_transaction(reqInfo);
	}
        break;
      case SC_REDIRECT_PERM:
        reqInfo->status = SC_REDIRECT_PERM;
	set_stat_line(reqInfo);
        if (((x=have_doc_error(reqInfo,type)) >= 0) && (!ErrorStat)) {
            ErrorStat = reqInfo->status;
            GoErrorDoc(reqInfo,x,err_string);
        } else {
            keep_alive.bKeepAlive = 0;
	    if(reqInfo->http_version != P_HTTP_0_9) {
	      strcpy(reqInfo->outh_location,err_string);
	      strcpy(reqInfo->outh_content_type,"text/html");
	      send_http_header(reqInfo);
	    }
            if (reqInfo->method != M_HEAD) {
              title_html(reqInfo,"Document moved");
              rprintf(reqInfo,"This document has permanently moved ");
              rprintf(reqInfo,"<A HREF=\"%s\">here</A>.<P>%c</BODY>%c",
		      err_string,LF,LF);
            }
            log_transaction(reqInfo);
        }
        break;
      case SC_USE_LOCAL_COPY:
        reqInfo->status = SC_USE_LOCAL_COPY;
	set_stat_line(reqInfo);
	if (reqInfo->http_version != P_HTTP_0_9) {
	  send_http_header(reqInfo);
	}
	keep_alive.bKeepAlive = 0;
	RetVal = SC_USE_LOCAL_COPY;
	log_transaction(reqInfo);
        break;
      case SC_AUTH_REQUIRED:
        reqInfo->status = SC_AUTH_REQUIRED;
	set_stat_line(reqInfo);
	strcpy(reqInfo->outh_www_auth, err_string);
	if (((x=have_doc_error(reqInfo,type)) >= 0) && (!ErrorStat)) {
	    ErrorStat = reqInfo->status;
	    GoErrorDoc(reqInfo,x,err_string);
	} else {
	    keep_alive.bKeepAlive = 0;
	    if(reqInfo->http_version != P_HTTP_0_9) {
	      strcpy(reqInfo->outh_content_type,"text/html");
	      send_http_header(reqInfo);
	    }
	    if (reqInfo->method != M_HEAD) {
	      title_html(reqInfo,"Authorization Required");
	      rprintf(reqInfo,"Browser not authentication-capable or %c",LF);
	      rprintf(reqInfo,"authentication failed.%c",LF);
              rprintf(reqInfo,"</BODY>%c",LF);
 	    }
	    log_transaction(reqInfo);
	}
        break;
      case SC_AUTH_NO_WWW_AUTH:
	reqInfo->status = SC_AUTH_REQUIRED;
	set_stat_line(reqInfo);
	keep_alive.bKeepAlive = 0;
        if(reqInfo->http_version != P_HTTP_0_9) {
          strcpy(reqInfo->outh_content_type,"text/html");
          send_http_header(reqInfo);
        }
        if (reqInfo->method != M_HEAD) {
          title_html(reqInfo,"Authorization Required");
          rprintf(reqInfo,"You are not permitted to get this URL: %c",LF);
          rprintf(reqInfo,"%s%c",err_string,LF);
          rprintf(reqInfo,"</BODY>%c",LF);
        }
        log_transaction(reqInfo);
        break;
      case SC_BAD_REQUEST:
        reqInfo->status = SC_BAD_REQUEST;
	set_stat_line(reqInfo);
	if (((x=have_doc_error(reqInfo,type)) >= 0) && (!ErrorStat)) {
	    ErrorStat = reqInfo->status;
	    GoErrorDoc(reqInfo,x,err_string);
	} else {
	    error_head(reqInfo,StatLine400);
	    keep_alive.bKeepAlive = 0;
	    if (reqInfo->method != M_HEAD) {
	      rprintf(reqInfo, 
		      "Your client sent a query that this server could%c",LF);
	      rprintf(reqInfo,"not understand.<P>%c",LF);
	      rprintf(reqInfo,"Reason: %s<P>%c",err_string,LF);
              rprintf(reqInfo,"</BODY>%c",LF);
	    } 
	    log_transaction(reqInfo);
	}
	break;
      case SC_BAD_IMAGEMAP:
        reqInfo->status = SC_BAD_REQUEST;
	set_stat_line(reqInfo);
	if (((x=have_doc_error(reqInfo,type)) >= 0) && (!ErrorStat)) {
	    ErrorStat = reqInfo->status;
	    GoErrorDoc(reqInfo,x,err_string);
	} else {
	    error_head(reqInfo,StatLine400);
	    keep_alive.bKeepAlive = 0;
	    if (reqInfo->method != M_HEAD) {
	      rprintf(reqInfo,
		      "Server encountered error processing imagemap%c",LF);
	      rprintf(reqInfo,"<P>Reason: %s<P>%c",err_string,LF);
              rprintf(reqInfo,"</BODY>%c",LF);
	    } 
	    log_transaction(reqInfo);
	}
	break;
      case SC_FORBIDDEN:
	if (reqInfo->outh_location[0])
	  reqInfo->status = SC_REDIRECT_TEMP;
	 else 
	  reqInfo->status = SC_FORBIDDEN;
	set_stat_line(reqInfo);
	if (((x=have_doc_error(reqInfo,type)) >= 0) && (!ErrorStat)) {
	    ErrorStat = reqInfo->status;
	    GoErrorDoc(reqInfo,x,err_string);
	} else {
	    error_head(reqInfo,StatLine403);
	    keep_alive.bKeepAlive = 0;
	    if (reqInfo->method != M_HEAD) {
	      rprintf(reqInfo,
		      "Your client does not have permission to get URL %s ",
		    err_string);
	      rprintf(reqInfo,"from this server.<P>%c",LF);
              rprintf(reqInfo,"</BODY>%c",LF);
	    }	
	    log_transaction(reqInfo);
	}
	break;
      case SC_NOT_FOUND:
        reqInfo->status = SC_NOT_FOUND;
	set_stat_line(reqInfo);
	if (((x=have_doc_error(reqInfo,type)) >= 0) && (!ErrorStat)) {
	    ErrorStat = reqInfo->status;
	    GoErrorDoc(reqInfo,x,err_string);
	} else {
	    error_head(reqInfo,StatLine404);
	    keep_alive.bKeepAlive = 0;
	    if (reqInfo->method != M_HEAD) {
	      rprintf(reqInfo,
		      "The requested URL %s was not found on this server.%c",
		      err_string,LF);
              rprintf(reqInfo,"</BODY>%c",LF);
	    } 
	    log_transaction(reqInfo);
	}
        break;
      case SC_SERVICE_UNAVAIL:
        reqInfo->status = SC_SERVICE_UNAVAIL;
        set_stat_line(reqInfo);
        if (((x=have_doc_error(reqInfo,type)) >= 0) && (!ErrorStat)) {
            ErrorStat = reqInfo->status;
            GoErrorDoc(reqInfo,x,err_string);
        } else {
            error_head(reqInfo,StatLine503);
            keep_alive.bKeepAlive = 0;
            if (reqInfo->method != M_HEAD) {
              rprintf(reqInfo,
                      "The requested URL %s is temporarily unavailable",
                      err_string);
              rprintf(reqInfo,"from this server.%c",LF);
              rprintf(reqInfo,"</BODY>%c",LF);
            }
            log_transaction(reqInfo);
        }
        break;
      case SC_SERVER_ERROR:
        reqInfo->status = SC_SERVER_ERROR;
	set_stat_line(reqInfo);
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
	      rprintf(reqInfo,"The server encountered an internal error or%c",LF);
	      rprintf(reqInfo,"misconfiguration and was unable to complete %c",LF);
	      rprintf(reqInfo,"your request.<P>%c",LF);
	      rprintf(reqInfo,"Please contact the server administrator,%c",LF);
	      rprintf(reqInfo," %s ",reqInfo->hostInfo->server_admin);
	      rprintf(reqInfo,"and inform them of the time the error occurred%c",LF);
	      rprintf(reqInfo,", and anything you might have done that may%c",LF);
	      rprintf(reqInfo,"have caused the error.<P>%c",LF);
	      rprintf(reqInfo,"<b>Error:</b> %s%c",err_string,LF);
              rprintf(reqInfo,"</BODY>%c",LF);
	    } 
	    log_transaction(reqInfo);
	}
        break;
      case SC_NOT_IMPLEMENTED:
        reqInfo->status = SC_NOT_IMPLEMENTED;
	set_stat_line(reqInfo);
	if (((x=have_doc_error(reqInfo,type)) >= 0) && (!ErrorStat)) {
	    ErrorStat = reqInfo->status;
	    GoErrorDoc(reqInfo,x,err_string);
	} else {
	    error_head(reqInfo,StatLine501);
	    keep_alive.bKeepAlive = 0;
	    if (reqInfo->method != M_HEAD) {
	      rprintf(reqInfo,"We are sorry to be unable to perform the method %s",
		    err_string);
	      rprintf(reqInfo," at this time or to this document.<P>%c",LF);
              rprintf(reqInfo,"</BODY>%c",LF);
	    }
	    log_transaction(reqInfo);
	}
        break;
      case SC_NO_MEMORY:
        log_error("HTTPd: memory exhausted",reqInfo->hostInfo->error_log);
        reqInfo->status = SC_SERVER_ERROR;
	set_stat_line(reqInfo);
        die_type = DIE_NORMAL;
	error_head(reqInfo,StatLine500);
	keep_alive.bKeepAlive = 0;
	if (reqInfo->method != M_HEAD) {
	    rprintf(reqInfo,"The server has temporarily run out of resources%c",LF);
	    rprintf(reqInfo,"for your request. Please try again at a later time.<P>%c",LF);
	    rprintf(reqInfo,"</BODY>%c",LF);
	}
	log_transaction(reqInfo);
      	break;
      case SC_CONF_ERROR:
	  reqInfo->status = SC_SERVER_ERROR;
	  set_stat_line(reqInfo);
	  sprintf(arguments,"HTTPd: configuration error = %s",err_string);
	  log_error(arguments,reqInfo->hostInfo->error_log);
	  error_head(reqInfo,StatLine500);
	  keep_alive.bKeepAlive = 0;
	  if (reqInfo->method != M_HEAD) {
	      rprintf(reqInfo,"The server has encountered a misconfiguration.%c",LF);
	      rprintf(reqInfo,"The error was %s.%c",err_string,LF);
	      rprintf(reqInfo,"</BODY>%c",LF);
	  }
	  log_transaction(reqInfo);
    }
    rflush(reqInfo);
    freeString(arguments);
    if (!RetVal) 
      htexit(reqInfo,1,die_type);
    return RetVal;
}

int GoErrorDoc(per_request *reqInfo, int x, char *ErrString) {
    per_request *newInfo;

    newInfo = continue_request(reqInfo, FORCE_GET | KEEP_ENV | KEEP_AUTH);
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
