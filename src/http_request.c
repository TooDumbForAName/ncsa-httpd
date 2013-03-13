/*
 * http_request.c: functions to get and process requests
 * 
 * Rob McCool 3/21/93
 * 
 * Include code by Charles Henrich
 */


#include "httpd.h"

#define OUTBUFSIZE 1024

int assbackwards;
char *remote_host;
char *remote_ip;
char *remote_name;

/* If RFC931 identity check is on, the remote user name */
char *remote_logname;

/*
 * Thanks to Charles Henrich
 */
void process_include(FILE *f, FILE *fd, char *incstring, char *args)
{
    FILE *ifp;
    char srcfile[HUGE_STRING_LEN];
    char command[HUGE_STRING_LEN];
    char errstr[MAX_STRING_LEN];
    char cmd[10];

    if(sscanf(incstring,"%s \"%[^\"]", cmd, srcfile) != 2) {
        sprintf(errstr,"the include string %s was invalid",incstring);
        die(INCLUDE_ERROR,errstr,fd);
    }                

    if(strncasecmp(cmd,"srv",3) != 0) {
        fprintf(fd, "%s\r\n",cmd);
        return;
    }

    if(srcfile[0] != '|') {
        if(num_includes > MAXINCLUDES) {
            die(INCLUDE_ERROR,
                "the maximum number of includes has been exceeded",fd);
        }

        num_includes++;

        if(translate_name(srcfile,fd) != 0)
            die(INCLUDE_ERROR,"non-standard file include",fd);
        ifp=fopen(srcfile,"r");

        if(ifp != NULL) {
            send_fd(ifp, fd, "");
            fclose(ifp);
        }
        else {
            sprintf(errstr,"the include file %s was invalid",srcfile);
            die(INCLUDE_ERROR,errstr,fd);
        }
    }
    else {
#ifndef PEM_AUTH
        escape_shell_cmd(&srcfile[1]);
        if(strncasecmp(cmd,"srvurl",6) == 0) {
            escape_shell_cmd(args);
            sprintf(command,"%s '%s'",&srcfile[1],args);
        }
        else {
            strcpy(command,&srcfile[1]);
        }

        ifp=popen(command,"r");

        if(ifp != NULL) {
            send_fd(ifp, fd, args);
            pclose(ifp);
        }
        else {
            sprintf(errstr,"the command %s is invalid",&srcfile[1]);
            die(INCLUDE_ERROR,errstr,fd);
        }
#else
        int pid,p[2];

        /* This is done because popen() seems to wait for the wrong */
        /* child process when encryption is being used. Suggestions */
        /* are welcome. */

        if(pipe(p) == -1)
            die(SERVER_ERROR,"Could not create an IPC pipe",fd);
        if((pid = fork()) == -1)
            die(SERVER_ERROR,"Could not fork a new process",fd);
        if(!pid) {
            char *argv0;
            char *cmd;

            cmd = &srcfile[1];
            if(!(argv0 = strrchr(cmd,'/')))
                argv0 = cmd;
            else
                argv0++;
            close(p[0]);
            if(p[1] != STDOUT_FILENO) {
                dup2(p[1],STDOUT_FILENO);
                close(p[1]);
            }
            if(strncasecmp(cmd,"srvurl",6) == 0) {
                escape_shell_cmd(args);
                if(execlp(cmd,argv0,args,(char *)0) == -1) {
                    char errstr[MAX_STRING_LEN];
                    sprintf(errstr,"could not execute included pgm %s",cmd);
                    die(INCLUDE_ERROR,errstr,fd);
                }
            }
            else {
                if(execlp(cmd,argv0,(char *)0) == -1) {
                    char errstr[MAX_STRING_LEN];
                    sprintf(errstr,"could not execute included pgm %s",cmd);
                    die(INCLUDE_ERROR,errstr,fd);
                }
            }
        } else {
            close(p[1]);
            if(!(ifp = fdopen(p[0],"r")))
                die(INCLUDE_ERROR,"could not open stream to include pipe",fd);
            send_fd(ifp,fd,args);
            fclose(ifp);
            waitpid(pid,NULL,0);
        }
#endif
    }
}

void send_fd_timed_out() {
    char errstr[MAX_STRING_LEN];

    sprintf(errstr,"httpd: send timed out for %s",remote_host);
    log_error(errstr);
    fclose(stdin);
    fclose(stdout);
    exit(0);
}

void send_fd(FILE *f, FILE *fd, char *args)
{
    int num_chars=0;
    char c;
    struct stat finfo;

    signal(SIGALRM,send_fd_timed_out);
    signal(SIGPIPE,send_fd_timed_out);

    if((allow_options & OPT_INCLUDES) && is_content_type("text/html")) {
        char *find="<inc";
        char incstring[MAX_STRING_LEN];
        int x,p;

        p=0;
        while(1) {
            alarm(timeout);
            c = fgetc(f);
            if(feof(f))
                return;
            if(tolower(c) == find[p]) {
                if((++p) == 4) {
                    x=0;
                    c=fgetc(f); /* get the space instead of the c */
                    while(c != '>') {
                        incstring[x++] = c;
                        c = fgetc(f);
                        if(feof(f)) {
                            incstring[x] = '\0';
                            fputs("<inc",fd);
                            fputs(incstring,fd);
                            return;
                        }
                    }
                    incstring[x] = '\0';
                    process_include(f,fd,incstring,args);
                    p=0;
                }
            }
            else {
                if(p) {
                    for(x=0;x<p;x++)
                        fputc(find[x],fd);
                    p=0;
                }
                fputc(c,fd);
            }
        }
    } else {
        char buf[OUTBUFSIZE];
        register int n,o,w;

        while (1) {
            alarm(timeout);
            if((n=fread(buf,sizeof(char),OUTBUFSIZE,f)) < 1) {
                break;
            }
            o=0;
            while(n) {
                w=fwrite(&buf[o],sizeof(char),n,fd);
                n-=w;
                o+=w;
            }
        }
    }
    fflush(fd);
}

void process_request(int in, FILE *out) {
    char m[HUGE_STRING_LEN];
    char w[HUGE_STRING_LEN];
    char l[HUGE_STRING_LEN];
    char url[HUGE_STRING_LEN];
    char args[HUGE_STRING_LEN];
#ifdef HENRICHS_SILLY_HACK
    char tmpargs[HUGE_STRING_LEN];
#endif
    int s,n;

    get_remote_host(in);
#ifdef PEM_AUTH
    doing_pem = -1;
  handle_request:
#endif
    l[0] = '\0';
    if(getline(l,HUGE_STRING_LEN,in,timeout))
        return;
    if(!l[0]) 
        return;

#ifdef PEM_AUTH
    if(doing_pem == -1)
#endif
    log_transaction(l);
    getword(m,l,' ');
    getword(args,l,' ');
#ifdef HENRICHS_SILLY_HACK
    strcpy(tmpargs,args);
    getword(url,tmpargs,';');
    if(!tempargs[0]) 
        getword(url,args,'?');
    else
        strcpy(args,tmpargs);
#else
    getword(url,args,'?');
#endif
    plustospace(url);
    unescape_url(url);
    getword(w,l,'\0');
    if(w[0] != '\0') {
        assbackwards = 0;
        get_mime_headers(in);
    }
    else
        assbackwards = 1;

    if(!strcmp(m,"HEAD")) {
        header_only=1;
        process_get(in,out,m,url,args);
    }
    else if(!strcmp(m,"GET")) {
#ifdef PEM_AUTH
        if(!assbackwards) {
            if(doing_pem == -1) {
                int s2;
                s2 = pem_decrypt(in,url,&out);
                if(s2 != -1) {
                    in = s2;
                    content_type[0] = '\0';
                    goto handle_request;
                }
            }
        }
#endif
        header_only=0;
        process_get(in,out,m,url,args);
    }
    else if(!strcmp(m,"POST")) {
        header_only = 0;
        get_node(url,args,in,out);
    }
    else 
        die(BAD_REQUEST,"Invalid or unsupported method.",out);

#ifdef PEM_AUTH
    if(doing_pem != -1) {
        close(in);
        htexit(0,out);
    }
#endif
}
