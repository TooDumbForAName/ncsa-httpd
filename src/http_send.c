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
 * http_send.c,v 1.34 1996/04/05 18:55:06 blong Exp
 *
 ************************************************************************
 *
 * http_send.c: handles sending of regular files and determining which
 *		type of request it is if its not for a regular file
 *
 * 
 */


#include "config.h"
#include "portability.h"

#include <stdio.h>
#ifndef NO_STDLIB_H
# include <stdlib.h>
#endif /* NO_STDLIB_H */
#ifdef HAVE_STDARG
# include <stdarg.h>
#else 
# ifdef HAVE_VARARGS
#  include <varargs.h>
# endif /* HAVE_VARARGS */
#endif /* HAVE_STDARG */
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <setjmp.h>
#include <ctype.h>
#include "constants.h"
#include "fdwrap.h"
#include "allocate.h"
#include "http_send.h"
#include "cgi.h"
#include "imagemap.h"
#include "http_mime.h"
#include "http_log.h"
#include "http_request.h"
#include "http_config.h"
#include "http_include.h"
#include "http_alias.h"
#include "http_access.h"
#include "http_dir.h"
#include "httpd.h"
#include "util.h"
#include "blackout.h"

static void (*exit_callback)(void);

void send_node(per_request *reqInfo) 
{
    struct stat finfo;
    register x = 0;
    int allow;
    char allow_options;
    int ErrReturn = 0;

    exit_callback = NULL;

/* It is no longer necessary to move all but one of the trailing slashes
 * to the path_info string, since all multiple slashes are now compressed
 * to one as a security precaution.
 */

    if(stat(reqInfo->filename,&finfo) == -1) {
	ErrReturn = extract_path_info(reqInfo,&finfo);
    }
    evaluate_access(reqInfo,&finfo,&allow,&allow_options);
    if (ErrReturn) {
        if(ErrReturn == ENOENT) {
	  log_reason(reqInfo,"file does not exist",reqInfo->filename);
	  die(reqInfo,SC_NOT_FOUND,reqInfo->url);
/* Check for AFS/NFS problems, and send back an unavailable message instead
 * Larry Schwimmer (schwim@cyclone.stanford.edu)
 */
        } else if ((ErrReturn == ETIMEDOUT) || (ErrReturn == ENODEV)) {
	  log_reason(reqInfo, "file temporarily unavailable",
	             reqInfo->filename);
	  die(reqInfo,SC_SERVICE_UNAVAIL,reqInfo->url);
	} else {
	  log_reason(reqInfo,"(3) file permissions deny server access",
		   reqInfo->filename);
	  die(reqInfo,SC_FORBIDDEN,reqInfo->url);
	}
    }
    if(!allow) {
        log_reason(reqInfo,"client denied by server configuration",
		   reqInfo->filename);
        die(reqInfo,SC_FORBIDDEN,reqInfo->url);
    }

    if (S_ISDIR(finfo.st_mode)) {
	send_dir(reqInfo,&finfo,allow_options);
    } else if (S_ISREG(finfo.st_mode)) {
	x = strlen(reqInfo->filename);
	/* Remove the trailing slash if its not a directory */
	if (reqInfo->filename[x-1] == '/') {
	  if (reqInfo->path_info[0] == '\0') {
	    reqInfo->path_info[0] = '/';
	    reqInfo->path_info[1] = '\0';
          }
	  reqInfo->filename[x-1] = '\0';
        }
	probe_content_type(reqInfo,reqInfo->filename);
	if (!strcmp(reqInfo->outh_content_type, CGI_MAGIC_TYPE))
	    send_cgi(reqInfo,&finfo,allow_options);
#ifdef IMAGEMAP_SUPPORT
	else if (!strcmp(reqInfo->outh_content_type, IMAGEMAP_MAGIC_TYPE))
	    send_imagemap(reqInfo,&finfo,allow_options);
#endif /* IMAGEMAP_SUPPORT */
	else
	    send_file(reqInfo,&finfo,allow_options);
    } else {
	log_reason(reqInfo,"improper file type",reqInfo->filename);
	/* device driver or pipe, no permission */
	die(reqInfo,SC_FORBIDDEN,reqInfo->url); 
    }
}

void send_file(per_request *reqInfo, struct stat *fi, char allow_options) 
{
    FILE *f;
#ifdef BLACKOUT_CODE
    int isblack = FALSE;
#endif /* BLACKOUT_CODE */    

    if ((reqInfo->method != M_GET) && (reqInfo->method != M_HEAD)) {
	sprintf(error_msg,"%s to non-script",methods[reqInfo->method]);
	die(reqInfo,SC_NOT_IMPLEMENTED,error_msg);
    }
    set_content_type(reqInfo,reqInfo->filename);

    if((allow_options & OPT_INCLUDES) && (!reqInfo->outh_content_encoding[0])) {
#ifdef XBITHACK
        if((fi->st_mode & S_IXUSR) ||
           (!strcmp(reqInfo->outh_content_type,INCLUDES_MAGIC_TYPE))) {
#else
	if(!strcmp(reqInfo->outh_content_type,INCLUDES_MAGIC_TYPE)) {
#endif /* XBITHACK */
	    reqInfo->bytes_sent = 0;
	    send_parsed_file(reqInfo, allow_options & OPT_INCNOEXEC);
	    log_transaction(reqInfo);
	    return;
	}
    }
    if (reqInfo->path_info[0]) {
	strcat(reqInfo->filename,reqInfo->path_info);
	strcat(reqInfo->url,reqInfo->path_info);
	sprintf(error_msg,"No file matching URL: %s",reqInfo->url);
	log_reason(reqInfo, error_msg, reqInfo->filename);
	die(reqInfo,SC_NOT_FOUND,reqInfo->url);
    }
	
    if(!(f=FOpen(reqInfo->filename,"r"))) {
      if (errno == EACCES) {
	log_reason(reqInfo,"(1) file permissions deny server access",
		   reqInfo->filename);
	/* we've already established that it exists */
	die(reqInfo,SC_FORBIDDEN,reqInfo->url); 
      } else {
	/* We know an error occured, of an unexpected variety. 
	 * This could be due to no more file descriptors.  We have this
	 * child exit after this stage so that errors of state are 
	 * swept under the carpet.
	 */
	standalone = 0;
	sprintf(error_msg,"File Open error, errno=%d",errno);
	log_reason(reqInfo,error_msg,reqInfo->filename);
	die(reqInfo,SC_SERVER_ERROR,error_msg);
      }
    }
    reqInfo->bytes_sent = 0;

#ifdef BLACKOUT_CODE
    if (!strcmp(reqInfo->outh_content_type,BLACKOUT_MAGIC_TYPE)) {
      isblack = TRUE;
      strcpy(reqInfo->outh_content_type,"text/html");
    }
#endif /* BLACKOUT_CODE */

    if(reqInfo->http_version != P_HTTP_0_9) {
      /* No length dependent headers since black is parsed */
#ifdef BLACKOUT_CODE
      if (isblack == FALSE) { 
#endif /* BLACKOUT_CODE */
#ifdef CONTENT_MD5
	reqInfo->outh_content_md5 = (unsigned char *)md5digest(f);
#endif /* CONTENT_MD5 */
	set_content_length(reqInfo,fi->st_size);
	if (set_last_modified(reqInfo,fi->st_mtime)) {
	    FClose(f);
	    return;
	}
      }
      if (reqInfo->http_version != P_HTTP_0_9) {
           send_http_header(reqInfo);
      }
#ifdef BLACKOUT_CODE
    }
#endif /* BLACKOUT_CODE */

    if(reqInfo->method != M_HEAD) {
#ifdef BLACKOUT_CODE
      if (isblack == TRUE)
	send_fp_black(reqInfo,f,NULL);
       else
#endif /* BLACKOUT_CODE */
	send_fp(reqInfo,f,NULL);
    }
    log_transaction(reqInfo);
    FClose(f);
}


void send_dir(per_request *reqInfo,struct stat *finfo, char allow_options) {
  char *name_ptr, *end_ptr;
  char *ifile, *temp_name;

  ifile = newString(HUGE_STRING_LEN,STR_TMP);
  temp_name = newString(HUGE_STRING_LEN,STR_TMP);

/* Path Alias (pa) array should now have the trailing slash */
  /*  if (pa[0] != '/') { */
  if ((reqInfo->filename[strlen(reqInfo->filename) - 1] != '/') && 
      (reqInfo->path_info[0] != '/')) {
    strcpy_dir(ifile,reqInfo->url);
    construct_url(temp_name,reqInfo->hostInfo,ifile);
    escape_url(temp_name);
    die(reqInfo,SC_REDIRECT_PERM,temp_name);
  }

  /* Don't allow PATH_INFO to directory indexes as a compromise for 
     error messages for files which don't exist */

  if ((reqInfo->path_info[0] != '\0') || (strlen(reqInfo->path_info) > 1)) {
        strcat(reqInfo->filename,reqInfo->path_info);
        strcat(reqInfo->url,reqInfo->path_info);
        sprintf(error_msg,"No file matching URL: %s",reqInfo->url);
        log_reason(reqInfo, error_msg, reqInfo->filename);
	freeString(temp_name);
	freeString(ifile);
        die(reqInfo,SC_NOT_FOUND,reqInfo->url);
  }
    
  strncpy(temp_name, reqInfo->hostInfo->index_names, HUGE_STRING_LEN-1);
  end_ptr = name_ptr = temp_name;

  while (*name_ptr) {
    
    while (*name_ptr && isspace (*name_ptr)) ++name_ptr;
    end_ptr = name_ptr;
    if (strchr(end_ptr, ' ') ) {
      end_ptr = strchr(name_ptr, ' ');
      *end_ptr = '\0';
      end_ptr++;
    } else
      end_ptr += strlen(end_ptr);
    make_full_path(reqInfo->filename,name_ptr,ifile);
    if(stat(ifile,finfo) == -1) {
      if(! *end_ptr && (allow_options & OPT_INDEXES)) {
        if (reqInfo->path_info[0]) {
          strcat(reqInfo->filename,reqInfo->path_info);
	  strcat(reqInfo->url,reqInfo->path_info);
	  log_reason(reqInfo,"file does not exist",reqInfo->filename);
	  freeString(ifile);
	  freeString(temp_name);
	  die(reqInfo,SC_NOT_FOUND,reqInfo->url);
	}
	if ((reqInfo->method != M_GET) && (reqInfo->method != M_HEAD)) {
	  sprintf(error_msg,"%s to non-script",methods[reqInfo->method]);
	  freeString(ifile);
	  freeString(temp_name);
	  die(reqInfo,SC_NOT_IMPLEMENTED,error_msg);
	}	
	index_directory(reqInfo);
	freeString(ifile);
	freeString(temp_name);
	return;
      } else if (! *end_ptr) {
	log_reason(reqInfo,"(2) file permissions deny server access",
		   reqInfo->filename);
	freeString(ifile);
	freeString(temp_name);
	die(reqInfo,SC_FORBIDDEN,reqInfo->url);
      }
    } else {
      strcpy(reqInfo->filename,ifile);
      probe_content_type(reqInfo,reqInfo->filename);
      if(!strcmp(reqInfo->outh_content_type,CGI_MAGIC_TYPE))
	send_cgi(reqInfo,finfo,allow_options);
      else
	send_file(reqInfo,finfo,allow_options);
      freeString(ifile);
      freeString(temp_name);
      return;
    }
    name_ptr = end_ptr;
  }	 
}

/* Search down given translated URL searching for actual file name and filling
 * in path_info string.  Doesn't make any claims about file type, must be 
 * handled elsewhere.
 * Returns 0 on success, errno on failure
 */
int extract_path_info(per_request *reqInfo, struct stat *finfo)
  {
    register int x,max;
    char *str;

    str = newString(HUGE_STRING_LEN,STR_TMP);

    max=count_dirs(reqInfo->filename);
    for(x=max ; x > 0 ; x--) {
        make_dirstr(reqInfo->filename,x+1,str);
        if(!(stat(str,finfo))) {
	  int l=strlen(str);
	  strcat(reqInfo->path_info,&(reqInfo->filename[l]));
	  reqInfo->filename[l] = '\0';
	  reqInfo->url[strlen(reqInfo->url) - strlen(reqInfo->path_info)]='\0';
	  freeString(str);
	  return 0;
        }
    }
    freeString(str);
    return errno;
}


/* Dump the headers of the per_request structure to the client.
 * Will also set the status line if it hasn't already been set,
 *  will set to 302 if location, 401 if auth required, 200 otherwise.
 * Will dump the following headers:
 * Date                         GMT Date in rfc 822 format
 * Server                       SERVER_VERSION
 * Annotations-cgi              reqInfo->hostInfo->annotation_server
 * Location                     reqInfo->outh_location
 * Last-modified                reqInfo->outh_last_mod
 * Content-type                 reqInfo->outh_content_type
 * Content-length               reqInfo->outh_content_length
 * Content-encoding             reqInfo->outh_content_encoding
 * Content-MD5                  reqInfo->outh_content_md5
 * WWW-Authenticate             reqInfo->outh_www_auth
 * Extension: Domain-Restricted reqInfo->bNotifyDomainRestricted && 
 *                              reqInfo->bSatisfiedDomain
 * Connection: Keep-Alive       keep_alive. stuff
 * Keep-Alive: max= timeout=    same
 * other headers from CGI       reqInfo->outh_cgi
 * We don't dump the MIME-Version header that NCSA HTTP/1.3 did, because
 * the server is not mime-compliant.
 *
 * Should we only give back HTTP/1.0 headers to 1.0 clients?  The docs are
 * unclear on this, and almost all clients (even ones supporting 1.1 features)
 * are sending HTTP/1.0 anyways, so we will for now.
 */

void send_http_header(per_request *reqInfo) 
{
    if(!reqInfo->status_line) {
       /* Special Cases */
        if(reqInfo->outh_location[0])
            reqInfo->status = SC_REDIRECT_TEMP;
	if(reqInfo->outh_www_auth[0])
	    reqInfo->status = SC_AUTH_REQUIRED;
	set_stat_line(reqInfo);
    }    
    rprintf(reqInfo,"%s %s%c%c",protocals[reqInfo->http_version],
	    reqInfo->status_line,CR,LF);
    rprintf(reqInfo,"Date: %s%c%c",gm_timestr_822(time(NULL)),CR,LF);
    rprintf(reqInfo,"Server: %s%c%c",SERVER_VERSION,CR,LF);
    if (reqInfo->hostInfo->annotation_server[0])
	rprintf(reqInfo,"Annotations-cgi: %s%c%c",
		reqInfo->hostInfo->annotation_server,CR,LF);

    if(reqInfo->outh_location[0])
        rprintf(reqInfo,"Location: %s%c%c",
		reqInfo->outh_location,CR,LF);
    if(reqInfo->outh_last_mod[0])
        rprintf(reqInfo,"Last-modified: %s%c%c",
		reqInfo->outh_last_mod,CR,LF);

    if(reqInfo->outh_content_type[0]) 
        rprintf(reqInfo,"Content-type: %s%c%c",
	        reqInfo->outh_content_type,CR,LF);

/* If we know the content_length, we can fulfill byte range requests */
    if(reqInfo->outh_content_length >= 0) {
/* Not yet, working on it */
/*	rprintf(reqInfo,"Accept-Ranges: bytes%c%c",CR,LF); */
        rprintf(reqInfo,"Content-length: %d%c%c",
		reqInfo->outh_content_length,CR,LF);
    }
    if(reqInfo->outh_content_encoding[0])
        rprintf(reqInfo,"Content-encoding: %s%c%c",
		reqInfo->outh_content_encoding,CR,LF);
#ifdef CONTENT_MD5
    if(reqInfo->outh_content_md5)
	rprintf(reqInfo,"Content-MD5: %s%c%c", 
		reqInfo->outh_content_md5,CR,LF);
#endif /* CONTENT_MD5 */

    if(reqInfo->outh_www_auth[0])
	rprintf(reqInfo,"WWW-Authenticate: %s%c%c", 
		reqInfo->outh_www_auth,CR,LF);

    if (reqInfo->bNotifyDomainRestricted && reqInfo->bSatisfiedDomain)
	rprintf(reqInfo,"Extension: Domain-Restricted%c%c",CR,LF);

    keep_alive.bKeepAlive = keep_alive.bKeepAlive && 
			    (reqInfo->outh_content_length >= 0);
    if (keep_alive.bKeepAlive && (!keep_alive.nMaxRequests ||
				  keep_alive.nCurrRequests + 1 < 
				  keep_alive.nMaxRequests)) {
	keep_alive.bKeepAlive = 1;
	rprintf(reqInfo,
		"Connection: Keep-Alive%c%cKeep-Alive: max=%d, timeout=%d%c%c",
		CR,LF, keep_alive.nMaxRequests, keep_alive.nTimeOut,CR,LF);
    }
    if(reqInfo->outh_cgi)
        rprintf(reqInfo,"%s",reqInfo->outh_cgi);

    rprintf(reqInfo,"%c%c",CR,LF);

/* CLF doesn't include the headers, I don't think, so clear the information
 * on what has been sent so far (byte count wise)
 */
    reqInfo->bytes_sent = 0;
/*    rflush(reqInfo);   */
}


/* Time out function for send_fd and send_fp
 * Logs whether an Abort or Time out occurs, logs how much has been sent,
 * Does some cleanup (CloseAll) and either exits of jumps back
 */


void send_fd_timed_out(int sigcode) 
{
    char *errstr;

    errstr = newString(HUGE_STRING_LEN,STR_TMP);
    if(exit_callback) (*exit_callback)();
    if (sigcode != SIGPIPE) {
	sprintf(errstr,"HTTPd: send timed out for %s, URL: %s", 
		(gCurrentRequest->remote_name ? 
		  gCurrentRequest->remote_name : "remote host"),
		(gCurrentRequest->url ? gCurrentRequest->url : "-"));
    }
    else {
	sprintf(errstr,"HTTPd: send aborted for %s, URL: %s", 
		(gCurrentRequest->remote_name ? 
		  gCurrentRequest->remote_name : "remote host"),
		(gCurrentRequest->url ? gCurrentRequest->url : "-"));
    }
    log_error(errstr,gCurrentRequest->hostInfo->error_log);
    log_transaction(gCurrentRequest);
    alarm(0);
    signal(SIGALRM,SIG_IGN);
    signal(SIGPIPE,SIG_IGN);
    CloseAll();  /* close all spurious file descriptors */

    if (!standalone) {
   	fclose(stdin); 
    	fclose(stdout); 
	exit(0);
    } else {
#ifdef NO_SIGLONGJMP
      longjmp(jmpbuffer,1);
#else
      siglongjmp(jmpbuffer,1);
#endif /* NO_SIGLONGJMP */
    }
}

/* send_fp(): sends a file pointer to the socket.  Uses fread to read,
 * but uses non-buffered I/O for writes (write())
 *
 * We'll make it return the number of bytes sent
 * so that we know if we need to send a body by default
 */
long send_fp(per_request *reqInfo, FILE *f, void (*onexit)(void))
{
    char *buf;
    long total_bytes_sent;
    register int n,o,w;

    buf = newString(IOBUFSIZE,STR_TMP);
    exit_callback = onexit;
    signal(SIGALRM,send_fd_timed_out);
    signal(SIGPIPE,send_fd_timed_out);

    total_bytes_sent = 0;
    rflush(reqInfo);
    while (1) {
        alarm(timeout);
        if((n=fread(buf,sizeof(char),IOBUFSIZE,f)) < 1) {
	   if (errno != EINTR) break;
        }

        o=0;
        if(reqInfo->bytes_sent != -1)
            reqInfo->bytes_sent += n;
        while(n) {
/*   Seems some systems have broken fwrite's (like AIX 3.2.5 on PowerPC)
 *   this should be a drop in replacement, maybe even be faster.
 *   For now, we'll just replace, but may have to #define one or the other
 *   depending on the system.
 */
              w = write(fileno(reqInfo->out),&buf[o],n);
	    if (w < 0) {
	      if (errno != EINTR) break;
	    }
            n-=w;
            o+=w;
	    total_bytes_sent += w;
        }
    }
    alarm(0);
    signal(SIGALRM,SIG_IGN);
    signal(SIGPIPE,SIG_IGN);
    freeString(buf);
    return total_bytes_sent;
}

/* rprintf() will print out to the socket, using buffered I/O
 */
int rprintf(per_request *reqInfo, char *format, ...)
{
   va_list argList;
   int x;

#ifndef HAVE_VARARGS
   va_start(argList,format);
#else
   va_start(argList);
#endif /* HAVE_VARARGS */
   x = vfprintf(reqInfo->out, format, argList);


   va_end(argList);

   reqInfo->bytes_sent += x;
   return x;

}

/* rputs() for drop into fputs(), for now.
 */
int rputs(char *string, per_request *reqInfo)
{
  reqInfo->bytes_sent += strlen(string);
  return fputs(string,reqInfo->out);
}

int rputc(int ch, per_request *reqInfo)
{ 
  (reqInfo->bytes_sent)++;
  return putc(ch, reqInfo->out);
}

int rflush(per_request *reqInfo)
{
  return fflush(reqInfo->out);
}
