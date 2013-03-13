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
 * http_include.c,v 1.41 1995/11/28 09:02:02 blong Exp
 *
 ************************************************************************
 *
 * http_include.c: Handles the server-parsed HTML documents
 *
 * Based on NCSA HTTPd 1.3 by Rob McCool
 * 
 *  04-07-95 blong
 *	Fixes bug where substrings of the environment variable might be 
 *	included first as suggested by David Robinson (drtr@ast.cam.ac.uk)
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
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include "constants.h"
#include "fdwrap.h"
#include "http_include.h"
#include "http_mime.h"
#include "http_log.h"
#include "http_config.h"
#include "http_request.h"
#include "http_send.h"
#include "cgi.h"
#include "http_access.h"
#include "http_alias.h"
#include "http_dir.h"
#include "util.h"
#include "env.h"

/* These are stored statically so that they can be reformatted quickly */
static time_t date,lm;

/* ------------------------ Environment function -------------------------- */

int add_include_vars(per_request *reqInfo, char *timefmt)
{
    struct stat finfo;
    char uri[HUGE_STRING_LEN];
    char *t; 

    date = time(NULL);
    make_env_str(reqInfo,"DATE_LOCAL",ht_time(date,timefmt,0));
    make_env_str(reqInfo,"DATE_GMT",ht_time(date,timefmt,1));

    if(stat(reqInfo->filename,&finfo) != -1) {
        lm = finfo.st_mtime;
        make_env_str(reqInfo,"LAST_MODIFIED",ht_time(lm,timefmt,0));
    }
    if((t = strrchr(reqInfo->filename,'/')))
      ++t;
    else
      t = reqInfo->url;
    make_env_str(reqInfo,"DOCUMENT_NAME",t);

    /* Jump through hoops because <=1.4 set DOCUMENT_URI to
       include index file name */
    if (reqInfo->url[strlen(reqInfo->url)-1] == '/' ){
      strncpy(uri,reqInfo->url,HUGE_STRING_LEN);
      strncat(uri,t,HUGE_STRING_LEN-strlen(uri));
      make_env_str(reqInfo,"DOCUMENT_URI",uri);
    } else {
      make_env_str(reqInfo,"DOCUMENT_URI",reqInfo->url);
    }


    return TRUE;
}

#define GET_CHAR(f,c,r) \
 { \
   int i = getc(f); \
   if(feof(f) || ferror(f) || (i == -1)) { \
        return r; \
   } \
   c = (char)i; \
 }

/* --------------------------- Parser functions --------------------------- */

int find_string(per_request *reqInfo, FILE *fp, char *str) {
    int x,l=strlen(str),p;
    char c;

    p=0;
    while(1) {
        GET_CHAR(fp,c,1);
        if(c == str[p]) {
            if((++p) == l)
                return 0;
        }
        else {
            if(reqInfo->out) {
                if(p) {
                    for(x=0;x<p;x++) {
                        putc(str[x],reqInfo->out);
                        ++(reqInfo->bytes_sent);
                    }
                }
                putc(c,reqInfo->out);
                ++(reqInfo->bytes_sent);
            }
            p=0;
        }
    }
}

char *get_tag(FILE *fp, char *tag) {
    char *t = tag, *tag_val, c;
    int n;

    n = 0;
    while(1) {
        GET_CHAR(fp,c,NULL);
        if(!isspace(c)) break;
    }
    /* problem: this drops tags starting with - or -- (tough s***) */
    if(c == '-') {
        GET_CHAR(fp,c,NULL);
        if(c == '-') {
            GET_CHAR(fp,c,NULL);
            if(c == '>') {
                strcpy(tag,"done");
                return tag;
            }
        }
    }
    /* this parser is very rigid, needs quotes around value and no spaces */
    while(1) {
        if(++n == MAX_STRING_LEN) {
            t[MAX_STRING_LEN - 1] = '\0';
            return NULL;
        }
        if((*t = c) == '\\') {
            GET_CHAR(fp,c,NULL);
            *t = c;
        } else if(*t == '=') {
            *t++ = '\0';
            tag_val = t;
            GET_CHAR(fp,c,NULL);
            if(c == '\"') {
                while(1) {
                    GET_CHAR(fp,c,NULL);
                    if(++n == MAX_STRING_LEN) {
                        t[MAX_STRING_LEN - 1] = '\0';
                        return NULL;
                    }
                    if((*t = c) == '\\') {
                        GET_CHAR(fp,c,NULL);
                        *t = c;
                    } else if(*t == '\"') {
                        *t = '\0';
                        return tag_val;
                    }
                    ++t;
                }
            } else 
                return NULL;
        }
        ++t;
        GET_CHAR(fp,c,NULL);
    }
}

int get_directive(FILE *fp, char *d) {
    char c;

    /* skip initial whitespace */
    while(1) {
        GET_CHAR(fp,c,1);
        if(!isspace(c))
            break;
    }
    /* now get directive */
    while(1) {
        *d++ = c;
        GET_CHAR(fp,c,1);
        if(isspace(c))
            break;
    }
    *d = '\0';
    return 0;
}

/* --------------------------- Action handlers ---------------------------- */


void send_parsed_content(per_request *reqInfo, FILE *f, char *path_args,
                         int noexec);

int send_included_file(per_request *reqInfo, char *fn) 
{
    FILE *fp;
    struct stat finfo;
    int allow;
    char op;

    if(stat(reqInfo->filename,&finfo) == -1)
        return -1;
    evaluate_access(reqInfo,&finfo,&allow,&op);
    if(!allow)
        return -1;
    set_content_type(reqInfo,reqInfo->filename);
    if((op & OPT_INCLUDES) && (!strcmp(content_type,INCLUDES_MAGIC_TYPE))) {
        if(!(fp = FOpen(reqInfo->filename,"r")))
            return -1;
        send_parsed_content(reqInfo,fp,"",op & OPT_INCNOEXEC);
        chdir_file(fn); /* grumble */
    }
    else if(!strcmp(content_type,CGI_MAGIC_TYPE))
        return -1;
    else {
        if(!(fp=FOpen(reqInfo->filename,"r")))
            return -1;
        send_fp(reqInfo,fp,NULL);
    }
    FClose(fp);
    return 0;
}

int handle_include(per_request *reqInfo, FILE *fp, char *error) {
    char tag[MAX_STRING_LEN],errstr[MAX_STRING_LEN];
    char *tag_val;

    while(1) {
        if(!(tag_val = get_tag(fp,tag)))
            return 1;
        if(!strcmp(tag,"file")) {
            char dir[MAX_STRING_LEN],to_send[MAX_STRING_LEN];
	    per_request *newInfo;

            getparents(tag_val); /* get rid of any nasties */
            getwd(dir);
            make_full_path(dir,tag_val,to_send);
	    newInfo = continue_request(reqInfo,NEW_URL | KEEP_ENV);
	    strcpy(newInfo->url,tag_val);
	    strcpy(newInfo->args,reqInfo->args);
	    strcpy(newInfo->filename,to_send);
            if(send_included_file(newInfo,reqInfo->filename)) {
                sprintf(errstr,"unable to include %s in parsed file %s",
                        newInfo->filename,reqInfo->filename );
                log_error(errstr,reqInfo->hostInfo->error_log);
                reqInfo->bytes_sent += fprintf(reqInfo->out,"%s",error);
            }            
	    reqInfo->bytes_sent += newInfo->bytes_sent;
	    free_request(newInfo,ONLY_LAST);
        } 
        else if(!strcmp(tag,"virtual")) {
	    per_request *newInfo;
	    newInfo = continue_request(reqInfo, NEW_URL | KEEP_ENV);
	    strcpy(newInfo->url,tag_val);
            if(translate_name(newInfo,newInfo->url,newInfo->filename) 
	       != A_STD_DOCUMENT) {
                reqInfo->bytes_sent += fprintf(reqInfo->out,"%s",error);
		sprintf(errstr,"unable to include %s in parsed file %s, non standard document",newInfo->filename, reqInfo->filename);
                log_error(errstr,reqInfo->hostInfo->error_log);
            } else {
		if(send_included_file(newInfo,reqInfo->filename)) {
                  sprintf(errstr,"unable to include %s in parsed file %s",
                        newInfo->filename, reqInfo->filename);
                  log_error(errstr,reqInfo->hostInfo->error_log);
                  reqInfo->bytes_sent += fprintf(reqInfo->out,"%s",error);
                }
	        reqInfo->bytes_sent += newInfo->bytes_sent;
	    }
	    free_request(newInfo,ONLY_LAST);
        } 
        else if(!strcmp(tag,"done"))
            return 0;
        else {
            sprintf(errstr,"unknown parameter %s to tag echo in %s",tag,
		    reqInfo->filename);
            log_error(errstr,reqInfo->hostInfo->error_log);
            reqInfo->bytes_sent += fprintf(reqInfo->out,"%s",error);
        }
    }
}

#ifndef NO_YOW
#include "httpy.h"

int print_yow(per_request *reqInfo, int yow_num) {
  int i = 0;
  int href_on = FALSE;
  int tmp;

  if (yow_num >= MAX_YOW) yow_num = MAX_YOW-1;
  while (yow_lines[yow_num][i]) {
    putc(yow_lines[yow_num][i],reqInfo->out);
    if (yow_lines[yow_num][i] == ' ') {
      tmp = href_on;
      href_on = (rand() % 100 < 50) ? 1 : 0;
      if (tmp != href_on) {
	if (!tmp) 
	  reqInfo->bytes_sent += fprintf(reqInfo->out,"<A HREF=\"%s\">",
					 reqInfo->url);
	 else 
	  reqInfo->bytes_sent += fprintf(reqInfo->out,"</A>");
      }
      i++;
      while ((yow_lines[yow_num][i] == ' ') && yow_lines[yow_num][i++]);
      i--;

    }
    (reqInfo->bytes_sent)++;
    i++;
  }
  if (href_on) {
    reqInfo->bytes_sent += fprintf(reqInfo->out,"</A>");
  }
}
 
#endif /* NO_YOW */
  
int handle_echo(per_request *reqInfo, FILE *fp, char *error) {
    char tag[MAX_STRING_LEN];
    char *tag_val;

    while(1) {
        if(!(tag_val = get_tag(fp,tag)))
            return 1;
        if(!strcmp(tag,"var")) {
            int x,i,len;

	    len = strlen(tag_val); 
            for(x=0;reqInfo->env[x] != NULL; x++) {
                i = ind(reqInfo->env[x],'=');
                if((i == len) && !(strncmp(reqInfo->env[x],tag_val,i))) {
                    reqInfo->bytes_sent += fprintf(reqInfo->out,"%s",
						   &(reqInfo->env[x][i+1]));
                    break;
                }
            }
            if(!(reqInfo->env[x])) 
	      reqInfo->bytes_sent += fprintf(reqInfo->out,"(none)");
        }
#ifndef NO_YOW
	else if(!strcmp(tag,"yow")) {
	   int num = atoi(tag_val);
	   print_yow(reqInfo,num);
        }
#endif /* NO_YOW */
        else if(!strcmp(tag,"done"))
            return 0;
        else {
            char errstr[MAX_STRING_LEN];
            sprintf(errstr,"unknown parameter %s to tag echo in %s",tag,
		    reqInfo->filename);
            log_error(errstr,reqInfo->hostInfo->error_log);
            reqInfo->bytes_sent += fprintf(reqInfo->out,"%s",error);
        }
    }
}

int include_cgi(per_request *reqInfo, char *pargs) {
    char op;
    int allow,check_cgiopt;
    struct stat finfo;

    getparents(reqInfo->url);
    if(reqInfo->url[0] == '/') {
        if(translate_name(reqInfo,reqInfo->url,reqInfo->filename) 
	   != A_SCRIPT_CGI) 
            return -1;
        check_cgiopt=0;
    } else {
        char dir[MAX_STRING_LEN];
        getwd(dir);
        make_full_path(dir,reqInfo->url,reqInfo->filename);
        check_cgiopt=1;
    }
    /* No hardwired path info or query allowed */
    if(stat(reqInfo->filename,&finfo) == -1)
        return -1;

    /* evaluate access */
    evaluate_access(reqInfo,&finfo,&allow,&op);
    if((!allow) || (check_cgiopt && (!(op & OPT_EXECCGI))))
        return -1;

    if(cgi_stub(reqInfo,pargs,&finfo) == SC_REDIRECT_TEMP)
        reqInfo->bytes_sent += fprintf(reqInfo->out,
			      "<A HREF=\"%s\">%s</A>",location,location);
    return 0;
}

static int ipid;
void kill_include_child(void) {
    char errstr[MAX_STRING_LEN];
    sprintf(errstr,"killing command process %d",ipid);
    log_error(errstr,gCurrentRequest->hostInfo->error_log);
    kill(ipid,SIGKILL);
    waitpid(ipid,NULL,0);
}

int include_cmd(per_request *reqInfo, char *s, char *pargs) {
    int p[2];
    FILE *fp;

    if(Pipe(p) == -1)
        die(reqInfo,SC_SERVER_ERROR,"httpd: could not create IPC pipe");
    if((ipid = fork()) == -1) {
	Close(p[0]);
	Close(p[1]);
        die(reqInfo,SC_SERVER_ERROR,"httpd: could not fork new process");
    }
    if(!ipid) {
        char *argv0;

        if(pargs[0] || reqInfo->args[0]) {
            if(pargs[0]) {
                char p2[HUGE_STRING_LEN];
                
                escape_shell_cmd(pargs);
                make_env_str(reqInfo,"PATH_INFO",pargs);
                translate_name(reqInfo,pargs,p2);
                make_env_str(reqInfo,"PATH_TRANSLATED",p2);
            }
            if(reqInfo->args[0]) {
                make_env_str(reqInfo,"QUERY_STRING",reqInfo->args);
                unescape_url(reqInfo->args);
                escape_shell_cmd(reqInfo->args);
                make_env_str(reqInfo,"QUERY_STRING_UNESCAPED",reqInfo->args);
            }
        }

        Close(p[0]);
        if(p[1] != STDOUT_FILENO) {
            dup2(p[1],STDOUT_FILENO);
            Close(p[1]);
        }
	close(reqInfo->connection_socket);
        error_log2stderr(reqInfo->hostInfo->error_log);
        if(!(argv0 = strrchr(SHELL_PATH,'/')))
            argv0=SHELL_PATH;
        if(execle(SHELL_PATH,argv0,"-c",s,(char *)0,reqInfo->env) == -1) {
            fprintf(stderr,"httpd: exec of %s failed, errno is %d\n",
                    SHELL_PATH,errno);
            exit(1);
        }
    }
    Close(p[1]);
/*    if(!(fp=FdOpen(p[0],"r"))) {
        waitpid(ipid,NULL,0);
        return -1;
    } */
    send_fd(reqInfo,p[0],kill_include_child);
/*    FClose(fp); */
    Close(p[0]);
    waitpid(ipid,NULL,0);
    return 0;
}


int handle_exec(per_request *reqInfo, FILE *fp, char *path_args, 
		char *error)
{
    char tag[MAX_STRING_LEN],errstr[MAX_STRING_LEN];
    char *tag_val;

    while(1) {
        if(!(tag_val = get_tag(fp,tag)))
            return 1;
        if(!strcmp(tag,"cmd")) {
            if(include_cmd(reqInfo,tag_val,path_args) == -1) {
                sprintf(errstr,"invalid command exec %s in %s",tag_val,
			reqInfo->filename);
                log_error(errstr,reqInfo->hostInfo->error_log);
                reqInfo->bytes_sent += fprintf(reqInfo->out,"%s",error);
            }
            /* just in case some stooge changed directories */
            chdir_file(reqInfo->filename);
        } 
        else if(!strcmp(tag,"cgi")) {
	    per_request *newInfo;
	    newInfo = continue_request(reqInfo,NEW_URL | KEEP_ENV);
	    strcpy(newInfo->url,tag_val);
	    
            if(include_cgi(newInfo,path_args) == -1) {
                sprintf(errstr,"invalid CGI ref %s in %s",newInfo->filename,
			reqInfo->filename);
                log_error(errstr,reqInfo->hostInfo->error_log);
                reqInfo->bytes_sent += fprintf(reqInfo->out,"%s",error);
            }
	    reqInfo->bytes_sent += newInfo->bytes_sent;
	    free_request(newInfo,ONLY_LAST);
            /* grumble groan */
            chdir_file(reqInfo->filename);
        }
        else if(!strcmp(tag,"done"))
            return 0;
        else {
            char errstr[MAX_STRING_LEN];
            sprintf(errstr,"unknown parameter %s to tag echo in %s",tag,
		    reqInfo->filename);
            log_error(errstr,reqInfo->hostInfo->error_log);
            reqInfo->bytes_sent += fprintf(reqInfo->out,"%s",error);
        }
    }

}

int handle_config(per_request *reqInfo, FILE *fp, char *error, 
		  char *tf, int *sizefmt) {
    char tag[MAX_STRING_LEN];
    char *tag_val;

    while(1) {
        if(!(tag_val = get_tag(fp,tag)))
            return 1;
        if(!strcmp(tag,"errmsg"))
            strcpy(error,tag_val);
        else if(!strcmp(tag,"timefmt")) {
            strcpy(tf,tag_val);
            /* Replace DATE* and LAST_MODIFIED (they should be first) */
	    replace_env_str(reqInfo, "DATE_LOCAL", ht_time(date,tf,0));
	    replace_env_str(reqInfo, "DATE_GMT", ht_time(date,tf,1));
	    replace_env_str(reqInfo, "LAST_MODIFIED", ht_time(lm,tf,0));
        }
        else if(!strcmp(tag,"sizefmt")) {
            if(!strcmp(tag_val,"bytes"))
                *sizefmt = SIZEFMT_BYTES;
            else if(!strcmp(tag_val,"abbrev"))
                *sizefmt = SIZEFMT_KMG;
        } 
        else if(!strcmp(tag,"done"))
            return 0;
        else {
            char errstr[MAX_STRING_LEN];
            sprintf(errstr,"unknown parameter %s to tag config in %s",
                    tag, reqInfo->filename);
            log_error(errstr,reqInfo->hostInfo->error_log);
            reqInfo->bytes_sent += fprintf(reqInfo->out,"%s",error);
        }
    }
}

#ifndef NO_YOW
int handle_yow(per_request *reqInfo, FILE *fp, char *error) {
    char tag[MAX_STRING_LEN];
    char c;

    srand((int) (getpid() + time((long *) 0)));  
    GET_CHAR(fp,c,1);
    if (c == ENDING_SEQUENCE[0]) {
        GET_CHAR(fp,c,1);
        if (c == ENDING_SEQUENCE[1]) {
	    GET_CHAR(fp,c,1);
	    if (c == ENDING_SEQUENCE[2]) {
		print_yow(reqInfo,rand() % MAX_YOW);
                return 0;
            } else return 1;
        } else return 1;
    } else return 1;
}
#endif /* NO_YOW */



int find_file(per_request *reqInfo, char *directive, char *tag, 
              char *tag_val, struct stat *finfo, char *error)
{
    char errstr[MAX_STRING_LEN], dir[MAX_STRING_LEN], to_send[MAX_STRING_LEN];

    if(!strcmp(tag,"file")) {
        getparents(tag_val); /* get rid of any nasties */
        getwd(dir);
        make_full_path(dir,tag_val,to_send);
        if(stat(to_send,finfo) == -1) {
            sprintf(errstr,
                    "unable to get information about %s in parsed file %s",
                    to_send,reqInfo->filename);
            log_error(errstr,reqInfo->hostInfo->error_log);
            reqInfo->bytes_sent += fprintf(reqInfo->out,"%s",error);
            return -1;
        }
        return 0;
    }
    else if(!strcmp(tag,"virtual")) {
	per_request *newInfo;
	newInfo = continue_request(reqInfo,NEW_URL | KEEP_ENV);
        strcpy(newInfo->url,tag_val);
        if(translate_name(newInfo,newInfo->url,newInfo->filename) 
	   != A_STD_DOCUMENT) {
	   sprintf(errstr,"unable to get information about non standard file %s in parsed file %s",newInfo->filename,reqInfo->filename);
            reqInfo->bytes_sent += fprintf(reqInfo->out,"%s",error);
            log_error(errstr,reqInfo->hostInfo->error_log);
        }  
        else if(stat(newInfo->filename,finfo) == -1) {
            sprintf(errstr,
                    "unable to get information about %s in parsed file %s",
                    newInfo->filename,reqInfo->filename);
            log_error(errstr,reqInfo->hostInfo->error_log);
            reqInfo->bytes_sent += fprintf(reqInfo->out,"%s",error);
	    free_request(newInfo,ONLY_LAST); 
            return -1;
        }
	free_request(newInfo,ONLY_LAST);
        return 0;
    }
    else {
        sprintf(errstr,"unknown parameter %s to tag %s in %s",
                tag,directive,reqInfo->filename);
        log_error(errstr,reqInfo->hostInfo->error_log);
        reqInfo->bytes_sent += fprintf(reqInfo->out,"%s",error);
        return -1;
    }
}


int handle_fsize(per_request *reqInfo, FILE *fp, char *error, int sizefmt)
{
    char tag[MAX_STRING_LEN];
    char *tag_val;
    struct stat finfo;

    while(1) {
        if(!(tag_val = get_tag(fp,tag)))
            return 1;
        else if(!strcmp(tag,"done"))
            return 0;
        else if(!find_file(reqInfo,"fsize",tag,tag_val,&finfo,error)) {
            if(sizefmt == SIZEFMT_KMG) {
                send_size(reqInfo,finfo.st_size);
                reqInfo->bytes_sent += 5;
            }
            else {
                int l,x;
                sprintf(tag,"%ld",(long)finfo.st_size);
                l = strlen(tag); /* grrr */
                for(x=0;x<l;x++) {
                    if(x && (!((l-x) % 3))) {
                        fputc(',',reqInfo->out);
                        ++reqInfo->bytes_sent;
                    }
                    fputc(tag[x],reqInfo->out);
                    ++reqInfo->bytes_sent;
                }
            }
        }
    }
}

int handle_flastmod(per_request *reqInfo, FILE *fp, char *error, char *tf)
{
    char tag[MAX_STRING_LEN];
    char *tag_val;
    struct stat finfo;

    while(1) {
        if(!(tag_val = get_tag(fp,tag)))
            return 1;
        else if(!strcmp(tag,"done"))
            return 0;
        else if(!find_file(reqInfo,"flastmod",tag,tag_val,&finfo,error))
            reqInfo->bytes_sent += fprintf(reqInfo->out,"%s",
					   ht_time(finfo.st_mtime,tf,0));
    }
}    



/* -------------------------- The main function --------------------------- */

/* This is a stub which parses a file descriptor. */

void send_parsed_content(per_request *reqInfo, FILE *fp, char *path_args,
                         int noexec)
{
    char directive[MAX_STRING_LEN], error[MAX_STRING_LEN];
    char timefmt[MAX_STRING_LEN], errstr[MAX_STRING_LEN];
    int ret, sizefmt;

    strcpy(error,DEFAULT_ERROR_MSG);
    strcpy(timefmt,DEFAULT_TIME_FORMAT);
    sizefmt = SIZEFMT_KMG;

    chdir_file(reqInfo->filename);

    while(1) {
        if(!find_string(reqInfo,fp,STARTING_SEQUENCE)) {
            if(get_directive(fp,directive))
                return;
            if(!strcmp(directive,"exec")) {
                if(noexec) {
                    sprintf(errstr,"httpd: exec used but not allowed in %s",
                            reqInfo->filename);
                    log_error(errstr,reqInfo->hostInfo->error_log);
                    reqInfo->bytes_sent += fprintf(reqInfo->out,"%s",error);
                    ret = find_string(reqInfo,fp,ENDING_SEQUENCE);
                } else 
                    ret=handle_exec(reqInfo,fp,path_args,error);
            } 
            else if(!strcmp(directive,"config"))
                ret=handle_config(reqInfo,fp,error,timefmt,&sizefmt);
            else if(!strcmp(directive,"include")) 
                ret=handle_include(reqInfo,fp,error);
            else if(!strcmp(directive,"echo"))
                ret=handle_echo(reqInfo,fp,error);
            else if(!strcmp(directive,"fsize"))
                ret=handle_fsize(reqInfo,fp,error,sizefmt);
            else if(!strcmp(directive,"flastmod"))
                ret=handle_flastmod(reqInfo,fp,error,timefmt);
#ifndef NO_YOW
	    else if(!strcmp(directive,"yow"))
		ret=handle_yow(reqInfo,fp,error);
#endif /* NO_YOW */
            else {
                sprintf(errstr,"httpd: unknown directive %s in parsed doc %s",
                        directive,reqInfo->filename);
                log_error(errstr,reqInfo->hostInfo->error_log);
                reqInfo->bytes_sent += fprintf(reqInfo->out,"%s",error);
                ret=find_string(reqInfo,fp,ENDING_SEQUENCE);
            }
            if(ret) {
                sprintf(errstr,"httpd: premature EOF in parsed file %s",
			reqInfo->filename);
                log_error(errstr,reqInfo->hostInfo->error_log);
                return;
            }
        } else 
            return;
    }
}

/* Called by send_file */

void send_parsed_file(per_request *reqInfo, char *path_args, int noexec) 
{
    FILE *fp;

    if(!(fp=FOpen(reqInfo->filename,"r"))) {
        log_reason(reqInfo,"file permissions deny server access",
		   reqInfo->filename);
        /* unmunge_name(reqInfo,reqInfo->filename); */
        die(reqInfo,SC_FORBIDDEN,reqInfo->url);
    }
    strcpy(content_type,"text/html");
    if(!no_headers)
        send_http_header(reqInfo);
    if(reqInfo->method == M_HEAD) {
	FClose(fp);
        return;
    }

    /* Make sure no children inherit our buffers */
    fflush(reqInfo->out);
    no_headers = TRUE; /* make sure no headers get inserted anymore */
    alarm(timeout);

    add_include_vars(reqInfo,DEFAULT_TIME_FORMAT);

    add_common_vars(reqInfo);

    send_parsed_content(reqInfo,fp,path_args,noexec);
    FClose(fp);
}
