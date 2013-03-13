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
 * http_send.c,v 1.22 1995/11/28 09:02:09 blong Exp
 *
 ************************************************************************
 *
 * http_send.c: handles sending of regular files and determining which
 *		type of request it is if its not for a regular file
 *
 * Based on NCSA HTTPd 1.3 by Rob McCool
 * 
 * 04-08-95  blong
 *	Fixed security hole which allowed a trailing slash on CGI_MAGIC_TYPE
 *	cgi anywhere scripts to send back the script contents.  Now the
 *	trailing slash is added to the PATH_INFO, and the script is run.
 *	Oh yeah, and don't forget about directories.
 *
 * 09-01-95  blong
 *	Fixed bug under AIX 3.2.5 where last part of file is garbled using
 *	fwrite, but works fine with write.  I didn't say I understood it,
 *	but the fix seems to work.
 */


#include "config.h"
#include "portability.h"

#include <stdio.h>
#ifndef NO_STDLIB_H
# include <stdlib.h>
#endif /* NO_STDLIB_H */
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <setjmp.h>
#include <ctype.h>
#include "constants.h"
#include "fdwrap.h"
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


int num_includes;

static void (*exit_callback)(void);

void send_node(per_request *reqInfo) 
{
    struct stat finfo;
    char pa[MAX_STRING_LEN];
    int length = 0;
    register x = 0;
    int allow;
    char allow_options;
    int ErrReturn = 0;

    exit_callback = NULL;

/* Remove all but 1 of the trailing slashes from the filename in order
   to fix security hole.  Place them in the path alias (pa) array */

 
    length = strlen(reqInfo->filename);
    while ((length>1) && (reqInfo->filename[length-1] == '/') && 
	   (reqInfo->filename[length-2] == '/') && (x < MAX_STRING_LEN)) {
	pa[x] = '/';
	x++;
	reqInfo->filename[length-1] = '\0';
	length--;
    }
    pa[x] = '\0';
    if(stat(reqInfo->filename,&finfo) == -1) {
	if ((ErrReturn = extract_path_info(reqInfo,pa,&finfo))) {
	    if(ErrReturn == ENOENT) {
		log_reason(reqInfo,"file does not exist",reqInfo->filename);
		die(reqInfo,SC_NOT_FOUND,reqInfo->url);
	    } else {
		log_reason(reqInfo,"(3) file permissions deny server access",
			   reqInfo->filename);
		die(reqInfo,SC_FORBIDDEN,reqInfo->url);
	    }
	}
    }
    evaluate_access(reqInfo,&finfo,&allow,&allow_options);
    if(!allow) {
        log_reason(reqInfo,"client denied by server configuration",
		   reqInfo->filename);
        die(reqInfo,SC_FORBIDDEN,reqInfo->url);
    }

    if (S_ISDIR(finfo.st_mode)) {
	send_dir(reqInfo,&finfo,pa,allow_options);
    } else if (S_ISREG(finfo.st_mode)) {
	probe_content_type(reqInfo,reqInfo->filename);
	if (!strcmp(content_type, CGI_MAGIC_TYPE))
	    send_cgi(reqInfo,&finfo,pa,allow_options);
#ifdef IMAGEMAP_SUPPORT
	else if (!strcmp(content_type, IMAGEMAP_MAGIC_TYPE))
	    send_imagemap(reqInfo,&finfo,pa,allow_options);
#endif /* IMAGEMAP_SUPPORT */
	else
	    send_file(reqInfo,&finfo,pa,allow_options);
    } else {
	log_reason(reqInfo,"improper file type",reqInfo->filename);
	/* device driver or pipe, no permission */
	die(reqInfo,SC_FORBIDDEN,reqInfo->url); 
    }
}

void send_file(per_request *reqInfo, struct stat *fi, 
               char *path_args, char allow_options) 
{
    FILE *f;

    if ((reqInfo->method != M_GET) && (reqInfo->method != M_HEAD)) {
	sprintf(error_msg,"%s to non-script",methods[reqInfo->method]);
	die(reqInfo,SC_NOT_IMPLEMENTED,error_msg);
    }
    set_content_type(reqInfo,reqInfo->filename);

    if((allow_options & OPT_INCLUDES) && (!content_encoding[0])) {
#ifdef XBITHACK
        if((fi->st_mode & S_IXUSR) ||
           (!strcmp(content_type,INCLUDES_MAGIC_TYPE))) {
#else
	if(!strcmp(content_type,INCLUDES_MAGIC_TYPE)) {
#endif /* XBITHACK */
	    reqInfo->bytes_sent = 0;
	    send_parsed_file(reqInfo,path_args,
			     allow_options & OPT_INCNOEXEC);
	    log_transaction(reqInfo);
	    return;
	}
    }
    if (path_args[0]) {
	strcat(reqInfo->filename,path_args);
	strcat(reqInfo->url,path_args);
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
    if(!no_headers) {
	set_content_length(reqInfo,fi->st_size);
	if (set_last_modified(reqInfo,fi->st_mtime)) {
	    FClose(f);
	    return;
	}
	send_http_header(reqInfo);
    }

    num_includes = 0;	
    if(!header_only) 
	send_fp(reqInfo,f,NULL);
    log_transaction(reqInfo);
    FClose(f);
}

/* Globals for speed */
static char ifile[HUGE_STRING_LEN];
static char temp_name[HUGE_STRING_LEN];

void send_dir(per_request *reqInfo,struct stat *finfo, char *pa, 
	      char allow_options) {
  char *name_ptr, *end_ptr;

/* Path Alias (pa) array should now have the trailing slash */
  /*  if (pa[0] != '/') { */
  if ((reqInfo->filename[strlen(reqInfo->filename) - 1] != '/') && 
      (pa[0] != '/')) {
    char url[HUGE_STRING_LEN];
    strcpy_dir(ifile,reqInfo->url);
    construct_url(url,reqInfo->hostInfo,ifile);
    escape_url(url);
    die(reqInfo,SC_REDIRECT_PERM,url);
  }

  /* Don't allow PATH_INFO to directory indexes as a compromise for 
     error messages for files which don't exist */

  if ((pa[0] != '\0') || (strlen(pa) > 1)) {
        strcat(reqInfo->filename,pa);
        strcat(reqInfo->url,pa);
        sprintf(error_msg,"No file matching URL: %s",reqInfo->url);
        log_reason(reqInfo, error_msg, reqInfo->filename);
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
        if (pa[0]) {
          strcat(reqInfo->filename,pa);
	  strcat(reqInfo->url,pa);
	  log_reason(reqInfo,"file does not exist",reqInfo->filename);
	  die(reqInfo,SC_NOT_FOUND,reqInfo->url);
	}
	if ((reqInfo->method != M_GET) && (reqInfo->method != M_HEAD)) {
	  sprintf(error_msg,"%s to non-script",methods[reqInfo->method]);
	  die(reqInfo,SC_NOT_IMPLEMENTED,error_msg);
	}	
	index_directory(reqInfo);
	return;
      } else if (! *end_ptr) {
	log_reason(reqInfo,"(2) file permissions deny server access",
		   reqInfo->filename);
	die(reqInfo,SC_FORBIDDEN,reqInfo->url);
      }
    } else {
      strcpy(reqInfo->filename,ifile);
      probe_content_type(reqInfo,reqInfo->filename);
      if(!strcmp(content_type,CGI_MAGIC_TYPE))
	send_cgi(reqInfo,finfo,pa,allow_options);
      else
	send_file(reqInfo,finfo,pa, allow_options);
      return;
    }
    name_ptr = end_ptr;
  }	 
}

/* Search down given translated URL searching for actual file name and filling
   in path_args string.  Doesn't make any claims about file type, must be 
   handled elsewhere.
   Returns 0 on success, errno on failure
   */
int extract_path_info(per_request *reqInfo, char *path_args,
		       struct stat *finfo)
  {
    register int x,max;
    char t[HUGE_STRING_LEN];

    max=count_dirs(reqInfo->filename);
    for(x=max ; x > 0 ; x--) {
        make_dirstr(reqInfo->filename,x+1,t);
        if(!(stat(t,finfo))) {
	  int l=strlen(t);
	  strcat(path_args,&(reqInfo->filename[l]));
	  reqInfo->filename[l] = '\0';
	  reqInfo->url[strlen(reqInfo->url) - strlen(path_args)] = '\0';
	  return 0;
        }
    }
    return errno;
}

void send_fd_timed_out(int sigcode) 
{
    char errstr[MAX_STRING_LEN];

    if(exit_callback) (*exit_callback)();
    if (sigcode != SIGPIPE) {
	sprintf(errstr,"httpd: send timed out for %s, URL: %s", 
	(gCurrentRequest->remote_name ?
          gCurrentRequest->remote_name : "remote host"),
	(gCurrentRequest->url ? gCurrentRequest->url : "-"));
    }
    else {
	sprintf(errstr,"httpd: send aborted for %s, URL: %s", 
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

/*
  We'll make it return the number of bytes sent
  so that we know if we need to send a body by default
*/
long send_fp(per_request *reqInfo, FILE *f, void (*onexit)(void))
{
    char buf[IOBUFSIZE];
    long total_bytes_sent;
    register int n,o,w;

    exit_callback = onexit;
    signal(SIGALRM,send_fd_timed_out);
    signal(SIGPIPE,send_fd_timed_out);

    total_bytes_sent = 0;
    fflush(reqInfo->out);
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
/*            w=fwrite(&buf[o],sizeof(char),n,reqInfo->out); */
	    if ((w=write(fileno(reqInfo->out),&buf[o],n)) < 0) {
	      if (errno != EINTR) break;
	    }
            n-=w;
            o+=w;
	    total_bytes_sent += w;
        }
    }
/*    fflush(reqInfo->out); */
    alarm(0);
    signal(SIGALRM,SIG_IGN);
    signal(SIGPIPE,SIG_IGN);
    return total_bytes_sent;
}
