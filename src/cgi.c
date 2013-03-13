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
 * cgi.c,v 1.34 1995/11/28 09:01:37 blong Exp
 *
 ************************************************************************
 *
 * cgi: keeps all script-related ramblings together.
 * 
 * Based on NCSA HTTPd 1.3 by Rob McCool
 *
 * 03-07-95 blong
 *	Added support for variable REMOTE_GROUP from access files
 *
 * 03-20-95 sguillory
 *	Moved to more dynamic memory management of environment arrays
 *
 * 04-03-95 blong
 *	Added support for variables DOCUMENT_ROOT, ERROR_REQINFO->STATUS
 *	ERROR_URL, ERROR_REQUEST
 *
 * 04-20-95 blong
 *      Added Apache patch "B18" from Rob Hartill to allow nondelayed redirects
 *
 * 05-02-95 blong
 *      Since Apache is using REDIRECT_ as the env variables, I've decided to
 *      go with this in the interest of general Internet Harmony and Peace.
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
#include "constants.h"
#include "fdwrap.h"
#include "cgi.h"
#include "env.h"
#include "http_request.h"
#include "http_log.h"
#include "http_access.h"
#include "http_mime.h"
#include "http_config.h"
#include "http_auth.h"
#include "http_alias.h"
#include "util.h"

int pid;

void kill_children(per_request *reqInfo) {
    char errstr[MAX_STRING_LEN];
    sprintf(errstr,"killing CGI process %d",pid);
    log_error(errstr,reqInfo->hostInfo->error_log);

    kill(pid,SIGTERM);
    sleep(3); /* give them time to clean up */
    kill(pid,SIGKILL);
    waitpid(pid,NULL,0);
}

#ifdef KRB4
# include <krb.h>
extern AUTH_DAT kerb_kdata;
# include <string.h>
#endif /* KRB4 */

char **create_argv(per_request *reqInfo,char *av0) {
    register int x,n;
    char **av;
    char w[HUGE_STRING_LEN];
    char l[HUGE_STRING_LEN];

    for(x=0,n=2;reqInfo->args[x];x++)
        if(reqInfo->args[x] == '+') ++n;

    if(!(av = (char **)malloc((n+1)*sizeof(char *))))
        die(reqInfo,SC_NO_MEMORY,"create_argv");
    av[0] = av0;
    strcpy(l,reqInfo->args);
    for(x=1;x<n;x++) {
        getword(w,l,'+');
        unescape_url(w);
        escape_shell_cmd(w);
        if(!(av[x] = strdup(w)))
            die(reqInfo,SC_NO_MEMORY,"create_argv");
    }
    av[n] = NULL;
    return av;
}

void get_path_info(per_request *reqInfo, char *path_args,
                   struct stat *finfo)
{
    register int x,max;
    char t[HUGE_STRING_LEN];

    path_args[0] = '\0';
    max=count_dirs(reqInfo->filename);
    for(x=dirs_in_alias;x<=max;x++) {
        make_dirstr(reqInfo->filename,x+1,t);
        if(!(stat(t,finfo))) {
            if(S_ISREG(finfo->st_mode)) {
                int l=strlen(t);
                strcpy(path_args,&(reqInfo->filename[l]));
                reqInfo->filename[l] = '\0';
		reqInfo->url[strlen(reqInfo->url) - strlen(path_args)] = '\0';
                return;
            }
        }
    }
    for(x=dirs_in_alias - 1; (x > 0) ;--x) {
        make_dirstr(reqInfo->filename,x+1,t);
        if(!(stat(t,finfo))) {
            if(S_ISREG(finfo->st_mode)) {
                strcpy(path_args,&(reqInfo->filename[strlen(t)]));
                strcpy(reqInfo->filename,t);
		reqInfo->url[strlen(reqInfo->url) - strlen(path_args)] = '\0';
                return;
            }
        }
    }
    /* unmunge_name(reqInfo,reqInfo->filename); */
    log_reason(reqInfo,"script does not exist",reqInfo->filename);
    die(reqInfo,SC_NOT_FOUND,reqInfo->url);
}

int add_cgi_vars(per_request *reqInfo, char *path_args, int *content)
{
    char t[HUGE_STRING_LEN];

    make_env_str(reqInfo,"GATEWAY_INTERFACE","CGI/1.1");

    make_env_str(reqInfo,"SERVER_PROTOCOL",
			    protocals[reqInfo->http_version]);
    make_env_str(reqInfo, "REQUEST_METHOD",
			    methods[reqInfo->method]);

    make_env_str(reqInfo,"SCRIPT_NAME",reqInfo->url);
    if(path_args[0]) {
        make_env_str(reqInfo,"PATH_INFO",path_args);
        translate_name(reqInfo,path_args,t);
        make_env_str(reqInfo,"PATH_TRANSLATED",t);
    }
    make_env_str(reqInfo,"QUERY_STRING",reqInfo->args);

    if(content) {
        *content=0;
	if ((reqInfo->method == M_POST) || (reqInfo->method == M_PUT)) {
            *content=1;
            sprintf(t,"%d",content_length);
            make_env_str(reqInfo,"CONTENT_TYPE",content_type_in);
            make_env_str(reqInfo,"CONTENT_LENGTH",t);
        }
    }

    return TRUE;
}

int add_common_vars(per_request *reqInfo) {
    char t[MAX_STRING_LEN],*env_path,*env_tz;


    if(!(env_path = getenv("PATH")))
        env_path=DEFAULT_PATH;
    make_env_str(reqInfo,"PATH",env_path);
    if((env_tz = getenv("TZ")))
        make_env_str(reqInfo,"TZ",env_tz);
    make_env_str(reqInfo,"SERVER_SOFTWARE",SERVER_VERSION);
    make_env_str(reqInfo,"SERVER_NAME",reqInfo->hostInfo->server_hostname);
    make_env_str(reqInfo,"SERVER_ADMIN",reqInfo->hostInfo->server_admin);

    sprintf(t,"%d",port);
    make_env_str(reqInfo,"SERVER_PORT",t);

    make_env_str(reqInfo,"REMOTE_HOST",reqInfo->remote_name);
    make_env_str(reqInfo,"REMOTE_ADDR",reqInfo->remote_ip);
    make_env_str(reqInfo,"DOCUMENT_ROOT",reqInfo->hostInfo->document_root);

    if(user[0])
      make_env_str(reqInfo,"REMOTE_USER",user);
    if(reqInfo->hostInfo->annotation_server[0])
      make_env_str(reqInfo,"ANNOTATION_SERVER",
		   reqInfo->hostInfo->annotation_server);
    if(groupname[0])
      make_env_str(reqInfo,"REMOTE_GROUP",groupname);

    if (reqInfo->auth_type[0]) {
#ifdef KRB4
      if(strncmp(reqInfo->auth_type, "kerberos", 8) == 0) {
	char buffer[1024];
	make_env_str(reqInfo,"AUTH_TYPE","KERB4_MUTUAL");
	make_env_str(reqInfo,"KERB4_USER",kerb_kdata.pname);
	make_env_str(reqInfo,"KERB4_INSTANCE",kerb_kdata.pinst);
	make_env_str(reqInfo,"KERB4_REALM",kerb_kdata.prealm);
	sprintf (buffer, "%s.%s@%s", kerb_kdata.pname, 
		 kerb_kdata.pinst, kerb_kdata.prealm);
	make_env_str(reqInfo,"KERB4_PRINCIPAL",buffer);
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

    return TRUE;
}

int scan_script_header(per_request *reqInfo, int pd) 
{
    char w[HUGE_STRING_LEN];
    char *l;
    int p;
    int nFirst = 1;
    int ret;

    /* Don't do keepalive unless the script returns a content-length 
       header */
    keep_alive.bKeepAlive = 0;
    while(1) {
        if((ret = getline(pd,w,HUGE_STRING_LEN-1,nFirst,timeout)) <= 0) {
	    char error_msg[MAX_STRING_LEN];
	    Close(pd);
	    sprintf(error_msg,"HTTPd: malformed header from script %s",
		    reqInfo->filename);
            die(reqInfo,SC_SERVER_ERROR,error_msg);
	}

	/* turn off forced read off socket */
	if (nFirst) nFirst = 0;

	/* Always return zero, so as not to cause redirect+sleep3+kill */
        if(w[0] == '\0') {
	    if (content_type[0] == '\0') {
	       if (location[0] != '\0') {
		 strcpy(content_type,"text/html");
	       } else {
	         if (local_default_type[0] != '\0')
		   strcpy(content_type,local_default_type);
	          else strcpy(content_type,reqInfo->hostInfo->default_type);
	       }
            }
	    return 0;
	}
        if(!(l = strchr(w,':')))
            l = w;
        *l++ = '\0';
        if(!strcasecmp(w,"Content-type")) {
	  /* Thanks Netscape for showing this bug to everyone */
	  /* delete trailing whitespace, esp. for "server push" */
	  char *endp = l + strlen(l) - 1;
	  while ((endp > l) && isspace(*endp)) *endp-- = '\0';
            sscanf(l,"%s",content_type);
        }
        else if(!strcasecmp(w,"Location")) {
        /* If we don't already have a status line, make one */
            if (!&status_line[0]) {
              reqInfo->status = 302;
              set_stat_line(reqInfo);
            }
            sscanf(l,"%s",location);
	}
        else if(!strcasecmp(w,"Status")) {
            for(p=0;isspace(l[p]);p++);
            sscanf(&l[p],"%d",&reqInfo->status);
            if(!(status_line = strdup(&l[p]))) {
		Close(pd);
                die(reqInfo,SC_NO_MEMORY,"CGI: scan_script_header");
	    }
        }
	else if(!strcasecmp(w,"Content-length")) {
	    keep_alive.bKeepAlive = 1;
	}		
        else {
            *(--l) = ':';
            for(p=0;w[p];p++);
            w[p] = LF;
            w[++p] = '\0';
            if(!out_headers) {
                if(!(out_headers = strdup(w))) {
		    Close(pd);
                    die(reqInfo,SC_NO_MEMORY,"CGI: scan_script_header");
		}
            }
            else {
                int loh = strlen(out_headers);
                out_headers = (char *) realloc(out_headers,
                                               (loh+strlen(w)+1)*sizeof(char));
                if(!out_headers) {
		    Close(pd);
                    die(reqInfo,SC_NO_MEMORY,"CGI: scan_script_header");
		}
                strcpy(&out_headers[loh],w);
            }
        }
    }
}

int cgi_stub(per_request *reqInfo, char *path_args, struct stat *finfo) 
{
    int p[2], p2[2];    /* p = script-> server, p2 = server -> script */
    int content, nph;
    char *argv0;
    char errlog[100];

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
        add_cgi_vars(reqInfo,path_args,&content);

	/* TAKE OUT "if (nph)" THROUGH "else {" IF SHIT HAPPENS */
	if (nph) {
	    if (reqInfo->connection_socket != STDOUT_FILENO) {
		dup2(reqInfo->connection_socket, STDOUT_FILENO);
	        close(reqInfo->connection_socket);
	    }
	}
	else
	    dup2(p[1],STDOUT_FILENO);
	Close(p[1]);   
	dup2(p2[0],STDIN_FILENO);
	Close(p2[0]);

/* Need to close the connection for processes which spawn processes.
 * is there a CLOSE_ON_EXEC_ON_EXEC ? */
/*	close(reqInfo->connection_socket); */
/*	fclose(reqInfo->out); */

        error_log2stderr(reqInfo->hostInfo->error_log);
	/* To make the signal handling work on HPUX, according to
	   David-Michael Lincke (dlincke@bandon.unisg.ch) */
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
	if (content_length > 0) {
	    /* read content off socket and write to script */
	    char szBuf[IOBUFSIZE];
	    int nBytes, nTotalBytes = 0;
	    int nDone = 0;

	    signal(SIGPIPE,SIG_IGN);
	    nBytes=getline(reqInfo->connection_socket, szBuf,IOBUFSIZE,2,
			   timeout);
	    nTotalBytes = nBytes;
	    write (p2[1], szBuf, nBytes);
	    while (!nDone && (nTotalBytes < content_length)) {
		if((nBytes=read(reqInfo->connection_socket, 
				szBuf,IOBUFSIZE)) < 1) {
		    break;
		}
		write (p2[1], szBuf, nBytes);
		nTotalBytes += nBytes;
	    }
	}
	Close(p2[1]);
    }   

    if(!nph) {
	content_type[0] = '\0';

        scan_script_header(reqInfo,p[0]);
        if(location[0] == '/') {
            char t[HUGE_STRING_LEN],a[HUGE_STRING_LEN],*argp;

            a[0] = '\0';
	    Close(p[0]);
            waitpid(pid,NULL,0);
            strcpy(t,location);
            if((argp = strchr(t,'?'))) {
                *argp++ = '\0';
                strcpy(a,argp);
            }
	    reqInfo->status = SC_REDIRECT_TEMP;
	    log_transaction(reqInfo);
	    reqInfo->status = SC_DOCUMENT_FOLLOWS;
            init_header_vars(reqInfo); /* clear location */
	    sprintf(the_request,"GET ");
	    strncat(the_request,t,HUGE_STRING_LEN - strlen(the_request));
	    if (a[0] != '\0') {
		strncat(the_request,"?",HUGE_STRING_LEN - strlen(the_request));
		strncat(the_request,a, HUGE_STRING_LEN - strlen(the_request));
	    }
	    
	    strncat(the_request," ",HUGE_STRING_LEN - strlen(the_request));
	    strncat(the_request, protocals[reqInfo->http_version], 
		    HUGE_STRING_LEN - strlen(the_request));
	    reqInfo = continue_request(reqInfo, KEEP_AUTH | FORCE_GET);
	    strcpy(reqInfo->url, t);
	    strcpy(reqInfo->args, a);
            process_request(reqInfo);
            return SC_REDIRECT_LOCAL;
        }
        content_length = -1;
        if(!no_headers)
            send_http_header(reqInfo);
        if(!header_only) {
	    /* Send a default body of text if the script
	       failed to produce any, but ONLY for redirects */
            if (!send_fd(reqInfo,p[0],NULL) && location[0]) {
		title_html(reqInfo,"Document moved");
		fprintf(reqInfo->out,
	  "This document has temporarily moved <A HREF=\"%s\">here</A>.</P>%c",
			 location,LF);
            }
        } else
            kill_children(reqInfo);
    }
    else {
	reqInfo->bytes_sent = -1;
	/* If there is KeepAlive going on, its handled internally to the 
	   script.  This means that we want to close the connection after
	   the nph- script has finished. */
	keep_alive.bKeepAlive = 0;
    }

    waitpid(pid,NULL,0);
    Close(p[0]);
    return SC_DOCUMENT_FOLLOWS;
}

/*
  We'll make it return the number of bytes sent
  so that we know if we need to send a body by default
*/
long send_fd(per_request *reqInfo, int pd, void (*onexit)(void))
{
    char buf[IOBUFSIZE];
    register int n,w,o;
    int fd;

    signal(SIGALRM,send_fd_timed_out);
    signal(SIGPIPE,send_fd_timed_out); 

    /* Flush stdio pipe, since scripts now use non buffered i/o */
    fflush(reqInfo->out);
    fd = fileno(reqInfo->out);

    alarm(timeout);
    n=getline(pd, buf,IOBUFSIZE,2,timeout);
    while (1) {
	o=0;
        while(n) {
	    if ((w=write(fd, buf + o,n)) < 1) {
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
    return reqInfo->bytes_sent;
}

/* Called for ScriptAliased directories */
void exec_cgi_script(per_request *reqInfo) {
    struct stat finfo;
    char path_args[HUGE_STRING_LEN];
    int stub_returns;
    int allow;
    char allow_options;

    get_path_info(reqInfo,path_args,&finfo);

    evaluate_access(reqInfo,&finfo,&allow,&allow_options);
    if(!allow) {
        log_reason(reqInfo,
		   "client denied by server configuration (CGI)",reqInfo->filename);
        /* unmunge_name(reqInfo,reqInfo->filename); */
        die(reqInfo,SC_FORBIDDEN,reqInfo->url);
    }
    add_common_vars(reqInfo);

    reqInfo->bytes_sent = 0;
    stub_returns = cgi_stub(reqInfo,path_args,&finfo);

    switch (stub_returns) {
	case SC_REDIRECT_TEMP:
		die(reqInfo,SC_REDIRECT_TEMP,location);
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

void send_cgi(per_request *reqInfo,struct stat *finfo, char *path_args, 
	      char allow_options) 
{
    int stub_returns;

    if (!(allow_options & OPT_EXECCGI)) {
        log_reason(reqInfo,"client denied by server configuration (CGI)",
		   reqInfo->filename);
        die(reqInfo,SC_FORBIDDEN,reqInfo->url);
    }
    add_common_vars(reqInfo);
    
    reqInfo->bytes_sent = 0;
    stub_returns = cgi_stub(reqInfo,path_args,finfo);

    switch (stub_returns) {
        case SC_REDIRECT_TEMP:
                die(reqInfo,SC_REDIRECT_TEMP,location);
                break;
        case SC_REDIRECT_LOCAL:
                break;
        default:
                log_transaction(reqInfo);
                break;
    }
}
