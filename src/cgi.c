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
 * cgi.c,v 1.44 1996/04/05 18:54:31 blong Exp
 *
 ************************************************************************
 *
 * cgi: keeps all script-related ramblings together.
 *
 */

#include "config.h"
#include "portability.h"

#include <stdio.h>
#ifndef NO_STDLIB_H
# include <stdlib.h>
#endif /* NO_STDLIB_H */
#include <ctype.h>
#ifndef NO_MALLOC_H
# ifdef NEED_SYS_MALLOC_H
#  include <sys/malloc.h>
# else
#  include <malloc.h>
# endif /* NEED_SYS_MALLOC_H */
#endif /* NO_MALLOC_H */
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/wait.h>
#include "constants.h"
#include "fdwrap.h"
#include "allocate.h"
#include "cgi.h"
#include "env.h"
#include "http_access.h"
#include "http_alias.h"
#include "http_auth.h"
#include "http_config.h"
#include "http_include.h"
#include "http_log.h"
#include "http_mime.h"
#include "http_request.h"
#include "http_send.h"
#include "http_include.h"
#include "httpd.h"
#include "util.h"


int pid;

#ifdef KRB4
# include <krb.h>
extern AUTH_DAT kerb_kdata;
# include <string.h>
#endif /* KRB4 */

void kill_children(per_request *reqInfo) {
    char errstr[MAX_STRING_LEN];
    sprintf(errstr,"killing CGI process %d",pid);
    log_error(errstr,reqInfo->hostInfo->error_log);

    kill(pid,SIGTERM);
    sleep(3); /* give them time to clean up */
    kill(pid,SIGKILL);
    waitpid(pid,NULL,0);
}

void kill_children_timed_out() {
   kill_children(gCurrentRequest);
}

char **create_argv(per_request *reqInfo,char *av0) {
    register int x,n;
    char **av;
    char *str1,*str2;

    str1 = newString(HUGE_STRING_LEN,STR_TMP);
    str2 = newString(HUGE_STRING_LEN,STR_TMP);

    for(x=0,n=2;reqInfo->args[x];x++)
        if(reqInfo->args[x] == '+') ++n;

    if(!(av = (char **)malloc((n+1)*sizeof(char *)))) {
	freeString(str1);
	freeString(str2);
        die(reqInfo,SC_NO_MEMORY,"create_argv");
    }
    av[0] = av0;
    strcpy(str2,reqInfo->args);
    for(x=1;x<n;x++) {
        getword(str1,str2,'+');
        unescape_url(str1);
        escape_shell_cmd(str1);
        if(!(av[x] = strdup(str1))) {
	    freeString(str1);
	    freeString(str2);
            die(reqInfo,SC_NO_MEMORY,"create_argv");
        }
    }
    av[n] = NULL;
    freeString(str1);
    freeString(str2);
    return av;
}

void get_path_info(per_request *reqInfo, struct stat *finfo)
{
    register int x,max;
    char *str;

    str = newString(HUGE_STRING_LEN,STR_TMP);

    max=count_dirs(reqInfo->filename);
    for(x=dirs_in_alias;x<=max;x++) {
        make_dirstr(reqInfo->filename,x+1,str);
        if(!(stat(str,finfo))) {
            if(S_ISREG(finfo->st_mode)) {
                int l=strlen(str);
                strcpy(reqInfo->path_info,&(reqInfo->filename[l]));
                reqInfo->filename[l] = '\0';
		reqInfo->url[strlen(reqInfo->url) - strlen(reqInfo->path_info)] = '\0';
		freeString(str);
                return;
            }
        }
    }
    for(x=dirs_in_alias - 1; (x > 0) ;--x) {
        make_dirstr(reqInfo->filename,x+1,str);
        if(!(stat(str,finfo))) {
            if(S_ISREG(finfo->st_mode)) {
                strcpy(reqInfo->path_info,&(reqInfo->filename[strlen(str)]));
                strcpy(reqInfo->filename,str);
		reqInfo->url[strlen(reqInfo->url) - strlen(reqInfo->path_info)] = '\0';
		freeString(str);
                return;
            }
        }
    }
    log_reason(reqInfo,"script does not exist",reqInfo->filename);
    freeString(str);
    die(reqInfo,SC_NOT_FOUND,reqInfo->url);
}

int add_cgi_vars(per_request *reqInfo, int *content)
{
    char *str;

    str = newString(HUGE_STRING_LEN,STR_TMP);

    make_env_str(reqInfo,"GATEWAY_INTERFACE","CGI/1.1");

    make_env_str(reqInfo,"SERVER_PROTOCOL",
			    protocals[reqInfo->http_version]);
    make_env_str(reqInfo, "REQUEST_METHOD",
			    methods[reqInfo->method]);

    make_env_str(reqInfo,"SCRIPT_NAME",reqInfo->url);
    if(reqInfo->path_info[0]) {
        make_env_str(reqInfo,"PATH_INFO",reqInfo->path_info);
        translate_name(reqInfo,reqInfo->path_info,str);
        make_env_str(reqInfo,"PATH_TRANSLATED",str);
    }
    make_env_str(reqInfo,"QUERY_STRING",reqInfo->args);

    if(content) {
        *content=0;
	if ((reqInfo->method == M_POST) || (reqInfo->method == M_PUT)) {
            *content=1;
            sprintf(str,"%d",reqInfo->inh_content_length);
            make_env_str(reqInfo,"CONTENT_TYPE",reqInfo->inh_content_type);
            make_env_str(reqInfo,"CONTENT_LENGTH",str);
        }
    }

    freeString(str);
    return TRUE;
}

int add_common_vars(per_request *reqInfo) {
    char *env_path,*env_tz;
    char *str;

    str = newString(HUGE_STRING_LEN,STR_TMP);

    if(!(env_path = getenv("PATH")))
        env_path=DEFAULT_PATH;
    make_env_str(reqInfo,"PATH",env_path);
    if((env_tz = getenv("TZ")))
        make_env_str(reqInfo,"TZ",env_tz);
    make_env_str(reqInfo,"SERVER_SOFTWARE",SERVER_VERSION);
    make_env_str(reqInfo,"SERVER_NAME",reqInfo->hostInfo->server_hostname);
    make_env_str(reqInfo,"SERVER_ADMIN",reqInfo->hostInfo->server_admin);

    sprintf(str,"%d",port);
    make_env_str(reqInfo,"SERVER_PORT",str);

    make_env_str(reqInfo,"REMOTE_HOST",reqInfo->remote_name);
    make_env_str(reqInfo,"REMOTE_ADDR",reqInfo->remote_ip);
    make_env_str(reqInfo,"DOCUMENT_ROOT",reqInfo->hostInfo->document_root);

    if(reqInfo->auth_user[0])
      make_env_str(reqInfo,"REMOTE_USER",reqInfo->auth_user);
    if(reqInfo->auth_group[0])
      make_env_str(reqInfo,"REMOTE_GROUP",reqInfo->auth_group);

    if(reqInfo->hostInfo->annotation_server[0])
      make_env_str(reqInfo,"ANNOTATION_SERVER",
		   reqInfo->hostInfo->annotation_server);

    if (reqInfo->auth_type[0]) {
#ifdef KRB4
      if(strncmp(reqInfo->auth_type, "kerberos", 8) == 0) {
	make_env_str(reqInfo,"AUTH_TYPE","KERB4_MUTUAL");
	make_env_str(reqInfo,"KERB4_USER",kerb_kdata.pname);
	make_env_str(reqInfo,"KERB4_INSTANCE",kerb_kdata.pinst);
	make_env_str(reqInfo,"KERB4_REALM",kerb_kdata.prealm);
	sprintf (str, "%s.%s@%s", kerb_kdata.pname, 
		 kerb_kdata.pinst, kerb_kdata.prealm);
	make_env_str(reqInfo,"KERB4_PRINCIPAL",str);
      } else
#endif /* KRB4 */
	make_env_str(reqInfo,"AUTH_TYPE",reqInfo->auth_type);
    }

    if(do_rfc931 && remote_logname)
        make_env_str(reqInfo,"REMOTE_IDENT",remote_logname);
    if (ErrorStat) {
      if (failed_request[0]) 
        make_env_str(reqInfo,"REDIRECT_REQUEST",failed_request);
      if (failed_url[0]) 
        make_env_str(reqInfo,"REDIRECT_URL",failed_url);
      make_env_str(reqInfo,"REDIRECT_STATUS",set_stat_line(reqInfo));
    }

    freeString(str);
    return TRUE;
}

int scan_cgi_header(per_request *reqInfo, int pd) 
{
    char *l;
    int p;
    int ret;
    int options = 0;
    char *str;

    str = newString(HUGE_STRING_LEN,STR_TMP); 

    /* Don't do keepalive unless the script returns a content-length 
       header */
    keep_alive.bKeepAlive = 0;

    reqInfo->cgi_buf = new_sock_buf(reqInfo,pd);
    cgibuf_count++;
   
  /* ADC put in the G_SINGLE_CHAR option, so that CGI SSI's would work.  
   * it was:
   * if((ret = getline(reqInfo->cgi_buf,str,HUGE_STRING_LEN-1,0,timeout)) <= 0)
   *
   * This should be cleaned up perhaps so that it only does this if SSI's are
   * allowed for this script directory.  ZZZZ
   */
#ifdef CGI_SSI_HACK
    if (reqInfo->RequestFlags & DOING_SHTTP)
	options = G_SINGLE_CHAR;
#endif /* CGI_SSI_HACK */

    while(1) {
      if((ret = getline(reqInfo->cgi_buf,str,HUGE_STRING_LEN-1,options,timeout)) <= 0)
      {
        char error_msg[MAX_STRING_LEN];
	Close(pd);
	freeString(str);
	sprintf(error_msg,"HTTPd: malformed header from script %s",
		reqInfo->filename);
        die(reqInfo,SC_SERVER_ERROR,error_msg);
      }

	/* Always return zero, so as not to cause redirect+sleep3+kill */
        if(str[0] == '\0') {
	    if (reqInfo->outh_content_type[0] == '\0') {
	       if (reqInfo->outh_location[0] != '\0') {
		 strcpy(reqInfo->outh_content_type,"text/html");
	       } else {
	         if (local_default_type[0] != '\0')
		   strcpy(reqInfo->outh_content_type,local_default_type);
	          else strcpy(reqInfo->outh_content_type,
			      reqInfo->hostInfo->default_type);
	       }
            }
	    freeString(str);
	    return 0;
	}
        if(!(l = strchr(str,':')))
            l = str;
        *l++ = '\0';
        if(!strcasecmp(str,"Content-type")) {
	  /* Thanks Netscape for showing this bug to everyone */
	  /* delete trailing whitespace, esp. for "server push" */
	  char *endp = l + strlen(l) - 1;
	  while ((endp > l) && isspace(*endp)) *endp-- = '\0';
            sscanf(l,"%s",reqInfo->outh_content_type);
        }
        else if(!strcasecmp(str,"Location")) {
        /* If we don't already have a status line, make one */
            if (!reqInfo->status_line) {
              reqInfo->status = SC_REDIRECT_TEMP;
              set_stat_line(reqInfo);
            }
	    strncpy(reqInfo->outh_location,l,HUGE_STRING_LEN);
	    reqInfo->outh_location[HUGE_STRING_LEN-1] = '\0';
	}
        else if(!strcasecmp(str,"Status")) {
            for(p=0;isspace(l[p]);p++);
            sscanf(&l[p],"%d",&(reqInfo->status));
            if(!(reqInfo->status_line = dupStringP(&l[p],STR_REQ))) {
		Close(pd);
		freeString(str);
                die(reqInfo,SC_NO_MEMORY,"CGI: scan_cgi_header");
	    }
        }
	else if(!strcasecmp(str,"Content-length")) {
	    keep_alive.bKeepAlive = 1;
	    sscanf(l,"%d",&(reqInfo->outh_content_length));
	}		
	else if(!strcasecmp(str,"WWW-Authenticate")) {
	   if (!reqInfo->status_line) {
	     reqInfo->status = SC_AUTH_REQUIRED;
	     set_stat_line(reqInfo);
           }
	   strncpy(reqInfo->outh_www_auth,l,HUGE_STRING_LEN);
	   reqInfo->outh_www_auth[HUGE_STRING_LEN-1] = '\0';
        }	
        else {
            *(--l) = ':';
            for(p=0;str[p];p++);
            str[p] = LF;
            str[++p] = '\0';
            if(!(reqInfo->outh_cgi)) {
                if(!(reqInfo->outh_cgi = strdup(str))) {
		    Close(pd);
		    freeString(str);
                    die(reqInfo,SC_NO_MEMORY,"CGI: scan_cgi_header");
		}
            }
            else {
                int loh = strlen(reqInfo->outh_cgi);
                reqInfo->outh_cgi = (char *) realloc(reqInfo->outh_cgi,
                                               (loh+strlen(str)+1)*sizeof(char));
                if(!(reqInfo->outh_cgi)) {
		    Close(pd);
		    freeString(str);
                    die(reqInfo,SC_NO_MEMORY,"CGI: scan_cgi_header");
		}
                strcpy(&(reqInfo->outh_cgi[loh]),str);
            }
        }
    }
}

void internal_redirect(per_request *reqInfo)
{
  char url[HUGE_STRING_LEN],args[HUGE_STRING_LEN],*argp;
  per_request *newInfo;
  
  url[0] = '\0';
  args[0] = '\0';

  /* Split the location header into URL and args */
  strncpy(url,reqInfo->outh_location,HUGE_STRING_LEN);
  if((argp = strchr(url,'?'))) {
    *argp++ = '\0';
    strcpy(args,argp);
  }
  log_transaction(reqInfo);

  /* The global string the_request currently holds the request as
   * read off the socket, and is used to log the information.  We force
   * it to this internal request here.  Only redirects to GET requests
   */
  sprintf(the_request,"GET ");
  strncat(the_request,url,HUGE_STRING_LEN - strlen(the_request));
  if (args[0] != '\0') {
    strncat(the_request,"?",HUGE_STRING_LEN - strlen(the_request));
    strncat(the_request,args, HUGE_STRING_LEN - strlen(the_request));
  }
  
  strncat(the_request," ",HUGE_STRING_LEN - strlen(the_request));
  /* Replace the protocal with Internal to let people know it was an
   * internal redirect.
   */
  /*  strncat(the_request, protocals[reqInfo->http_version], 
	  HUGE_STRING_LEN - strlen(the_request)); */
  strncat(the_request, "Internal",HUGE_STRING_LEN - strlen(the_request));
  newInfo = continue_request(reqInfo, KEEP_AUTH | FORCE_GET);
  newInfo->status = SC_DOCUMENT_FOLLOWS;
  set_stat_line(newInfo);
  strcpy(newInfo->url, url);
  strcpy(newInfo->args, args);
  construct_url(newInfo->outh_location,reqInfo->hostInfo,
		reqInfo->outh_location);
  process_request(newInfo);
}

int cgi_stub(per_request *reqInfo, struct stat *finfo, int allow_options) 
{
  int p[2], p2[2];    /* p = script-> server, p2 = server -> script */
  int content, nph;
  char *argv0;
  char errlog[100];
  FILE *fp = NULL;
  
  if(!can_exec(finfo)) {
    log_reason(reqInfo,
	       "client denied by server configuration (CGI non-executable)",
	       reqInfo->filename);
    die(reqInfo,SC_FORBIDDEN,reqInfo->url);
  }
  
  if((argv0 = strrchr(reqInfo->filename,'/')) != NULL)
    argv0++;
  else argv0 = reqInfo->filename;
  
  chdir_file(reqInfo->filename);
  
  if(Pipe(p) < 0)
    die(reqInfo,SC_SERVER_ERROR,"HTTPd/CGI: could not create IPC pipe");
  if(Pipe(p2) < 0) {
    Close(p[0]);
    Close(p[1]);
    die(reqInfo,SC_SERVER_ERROR,"HTTPd/CGI: could not create IPC pipe");
  }
  
  if((pid = fork()) < 0) {
    Close(p[0]);
    Close(p[1]);
    Close(p2[0]);
    Close(p2[1]);
    sprintf(errlog,"HTTPd/CGI: could not fork new process, errno is %d",
	    errno);
    die(reqInfo,SC_SERVER_ERROR,errlog);
  }
  
  nph = (strncmp(argv0,"nph-",4) ? 0 : 1);
  if(!pid) {
    Close(p[0]);
    Close(p2[1]);
    standalone = 0; 
    add_cgi_vars(reqInfo,&content);
    
    /* TAKE OUT "if (nph)" THROUGH "else {" IF SHIT HAPPENS */
    if (nph) {
      if (fileno(reqInfo->out) != STDOUT_FILENO) {
	dup2(fileno(reqInfo->out), STDOUT_FILENO);
	close(fileno(reqInfo->out));
      }
    }
    else
      dup2(p[1],STDOUT_FILENO);
    Close(p[1]);   
    dup2(p2[0],STDIN_FILENO);
    Close(p2[0]);

    /* Close the socket so the CGI program doesn't hold it open */
    close(csd);
    
    error_log2stderr(reqInfo->hostInfo->error_log);
    /* To make the signal handling work on HPUX, according to
     * David-Michael Lincke (dlincke@bandon.unisg.ch) 
     */
#ifdef HPUX
    signal(SIGCHLD, SIG_DFL);
#endif /* HPUX */
    /* Only ISINDEX scripts get decoded arguments. */
    if((!reqInfo->args[0]) || (ind(reqInfo->args,'=') >= 0)) {
      execle(reqInfo->filename,argv0,NULL,reqInfo->env);
    }
    else {
      execve(reqInfo->filename,create_argv(reqInfo,argv0),
	     reqInfo->env);
      
    }
    fprintf(stderr,"HTTPd/CGI: exec of %s failed, errno is %d\n",
	    reqInfo->filename,errno);
    exit(1);
  }
  else {
    Close(p[1]);
    Close(p2[0]);
    if (reqInfo->inh_content_length > 0) {
      /* read content off socket and write to script */
      char szBuf[HUGE_STRING_LEN];
      int nBytes, nTotalBytes = 0;
      int nDone = 0;
      
      signal(SIGPIPE,SIG_IGN);
      nBytes=getline(reqInfo->sb, szBuf,HUGE_STRING_LEN,G_FLUSH, timeout);
      nTotalBytes = nBytes;
      if (nBytes >= 0) {
        if (nBytes > 0) write(p2[1], szBuf, nBytes);
        while (!nDone && (nTotalBytes < reqInfo->inh_content_length)) {
	    nBytes=read(reqInfo->in, szBuf,HUGE_STRING_LEN);
	  if(nBytes < 1) {
	    break;
	  }
	  write(p2[1], szBuf, nBytes);
	  nTotalBytes += nBytes;
        }
      }
    }
    Close(p2[1]);
  }   
  
  if(!nph) {
    reqInfo->outh_content_type[0] = '\0';
    reqInfo->outh_content_length = -1;
    
    scan_cgi_header(reqInfo,p[0]);
    if(reqInfo->outh_location[0] == '/') {
      Close(p[0]);
      waitpid(pid,NULL,0);
      internal_redirect(reqInfo);
      return SC_REDIRECT_LOCAL;
    }
    
    /* Previously, this was broken because we read the results of the CGI using
     * getline, but the SSI parser used buffered stdio.
     * 
     * ADC changed scan_cgi_header so that it uses G_SINGLE_CHAR when it
     * calls getline.  Yes, this means pitiful performance for CGI scripts.
     */
    /* Fine, parse the output of CGI scripts.  Talk about useless
     * overhead. . .
     */
#ifdef CGI_SSI_HACK
    if (!strcasecmp(reqInfo->outh_content_type, INCLUDES_MAGIC_TYPE) &&
	(allow_options & OPT_INCLUDES)) {
      strcpy(reqInfo->outh_content_type, "text/html");
      if(reqInfo->http_version != P_HTTP_0_9)
	send_http_header(reqInfo);
      if(reqInfo->method != M_HEAD) {
	rflush(reqInfo);
	alarm(timeout);
	add_include_vars(reqInfo,DEFAULT_TIME_FORMAT);
	if (!(fp = FdOpen(p[0],"r"))) {
	  char errstr[MAX_STRING_LEN];
	  sprintf(errstr,"HTTPd/CGI/SSI: Could not fdopen() fildes, errno is %d.",errno);
	  die(reqInfo,SC_SERVER_ERROR,errstr);
	}
	send_parsed_content(reqInfo,fp,allow_options & OPT_EXECCGI);
      }
        } 
    else
#endif /* CGI_SSI_HACK */
      { /* Not Parsed, send normally */
	if(reqInfo->http_version != P_HTTP_0_9)
	  send_http_header(reqInfo);
	if(reqInfo->method != M_HEAD) {
	  /* Send a default body of text if the script
	   * failed to produce any, but ONLY for redirects 
	   */
	  if (!send_fd(reqInfo,p[0],kill_children_timed_out) && 
	      reqInfo->outh_location[0]) 
	  {
	    title_html(reqInfo,"Document moved");
	    rprintf(reqInfo,
		    "This document has temporarily moved <A HREF=\"%s\">here</A>.</P>%c",
		    reqInfo->outh_location,LF);
	  }
	} else
	  kill_children(reqInfo);
      }
  }
  else { /* Is nph- script */
    reqInfo->bytes_sent = -1;
    /* If there is KeepAlive going on, its handled internally to the 
       script.  This means that we want to close the connection after
       the nph- script has finished. */
    keep_alive.bKeepAlive = 0;
  }
  
  waitpid(pid,NULL,0);
  if (fp != NULL) FClose(fp); else Close(p[0]);
  return SC_DOCUMENT_FOLLOWS;
}

/*
  We'll make it return the number of bytes sent
  so that we know if we need to send a body by default
*/
long send_fd(per_request *reqInfo, int pd, void (*onexit)(void))
{
    char *buf;
    register int n,w,o;
    int fd;

    buf = newString(IOBUFSIZE,STR_TMP);

    exit_callback = onexit;
    signal(SIGALRM,send_fd_timed_out);
    signal(SIGPIPE,send_fd_timed_out); 

    /* Flush stdio pipe, since scripts now use non buffered i/o */
    rflush(reqInfo);
    fd = fileno(reqInfo->out);

    alarm(timeout);
    if (reqInfo->cgi_buf != NULL)
      n=getline(reqInfo->cgi_buf, buf,IOBUFSIZE,G_FLUSH,timeout);
     else 
      n = 0;
    while (1) {
	o=0;
        while(n) {
	    w = write(fd, buf + o,n);

	    if (w < 1) {
	      if (errno != EINTR) break;
	    }
            n-=w;
	    o+=w;
	    reqInfo->bytes_sent += w;
        }
        if((n=read(pd, buf,IOBUFSIZE)) < 1) {
	    if (n == -1 && errno == EINTR) {
	      n = 0;
	      continue;
	    }
            break;
	}
    }
    alarm(0);
    signal(SIGALRM,SIG_IGN);
    signal(SIGPIPE,SIG_IGN);
    freeString(buf);
    return reqInfo->bytes_sent;
}

/* Called for ScriptAliased directories */
void exec_cgi_script(per_request *reqInfo) {
    struct stat finfo;
    int stub_returns;
    int allow;
    char allow_options;

    get_path_info(reqInfo,&finfo);

    evaluate_access(reqInfo,&finfo,&allow,&allow_options);
    if(!allow) {
        log_reason(reqInfo,
		   "client denied by server configuration (CGI)",reqInfo->filename);
        /* unmunge_name(reqInfo,reqInfo->filename); */
        die(reqInfo,SC_FORBIDDEN,reqInfo->url);
    }
    add_common_vars(reqInfo);

    reqInfo->bytes_sent = 0;
    stub_returns = cgi_stub(reqInfo,&finfo,allow_options);

    switch (stub_returns) {
	case SC_REDIRECT_TEMP:
		die(reqInfo,SC_REDIRECT_TEMP,reqInfo->outh_location);
		break;
	case SC_REDIRECT_LOCAL:
		break;
	default:
		log_transaction(reqInfo);
		break;
    }
}

/* Almost exactly equalivalent to exec_cgi_script, but this one
   gets all of the path info passed to it, instead of calling get_path_info 
   and also gets the allow information passed to it instead of calling
   evaluate_access 
*/

void send_cgi(per_request *reqInfo,struct stat *finfo, char allow_options) 
{
    int stub_returns;

    if (!(allow_options & OPT_EXECCGI)) {
        log_reason(reqInfo,"client denied by server configuration (CGI)",
		   reqInfo->filename);
        die(reqInfo,SC_FORBIDDEN,reqInfo->url);
    }
    add_common_vars(reqInfo);
    
    reqInfo->bytes_sent = 0;
    stub_returns = cgi_stub(reqInfo,finfo,allow_options);

    switch (stub_returns) {
        case SC_REDIRECT_TEMP:
                die(reqInfo,SC_REDIRECT_TEMP,reqInfo->outh_location);
                break;
        case SC_REDIRECT_LOCAL:
                break;
        default:
                log_transaction(reqInfo);
                break;
    }
}
