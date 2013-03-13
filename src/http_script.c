/*
 * http_script: keeps all script-related ramblings together.
 * 
 * All code contained herein is covered by the Copyright as distributed
 * in the README file in the main directory of the distribution of 
 * NCSA HTTPD.
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
 *	Added support for variables DOCUMENT_ROOT, ERROR_STATUS
 *	ERROR_URL, ERROR_REQUEST
 *
 * 04-20-95 blong
 *	Added Apache patch "B18" from Rob Hartill to allow nondelayed redirects
 *
 * 05-02-95 blong
 *    Since Apache is using REDIRECT_ as the env variables, I've decided to 
 *    go with this in the interest of general Internet Harmony and Peace.
 */

#include "httpd.h"
#include "new.h"

int pid;

void kill_children() {
    char errstr[MAX_STRING_LEN];
    sprintf(errstr,"killing CGI process %d",pid);
    log_error_noclose(errstr);

    kill(pid,SIGTERM);
    sleep(3); /* give them time to clean up */
    kill(pid,SIGKILL);
    waitpid(pid,NULL,0);
}

char **create_argv(char *av0, char *args, FILE *out) {
    register int x,n;
    char **av;
    char w[HUGE_STRING_LEN];
    char l[HUGE_STRING_LEN];

    for(x=0,n=2;args[x];x++)
        if(args[x] == '+') ++n;

    if(!(av = (char **)malloc((n+1)*sizeof(char *))))
        die(NO_MEMORY,"create_argv",out);
    av[0] = av0;
    strcpy(l,args);
    for(x=1;x<n;x++) {
        getword(w,l,'+');
        unescape_url(w);
        escape_shell_cmd(w);
        if(!(av[x] = strdup(w)))
            die(NO_MEMORY,"create_argv",out);
    }
    av[n] = NULL;
    return av;
}

void get_path_info(char *path, char *path_args, FILE *out, 
                   struct stat *finfo)
{
    register int x,max;
    char t[HUGE_STRING_LEN];

    path_args[0] = '\0';
    max=count_dirs(path);
    for(x=dirs_in_alias;x<=max;x++) {
        make_dirstr(path,x+1,t);
        if(!(stat(t,finfo))) {
            if(S_ISREG(finfo->st_mode)) {
                int l=strlen(t);
                strcpy(path_args,&path[l]);
                path[l] = '\0';
                return;
            }
        }
    }
    for(x=dirs_in_alias - 1;x;--x) {
        make_dirstr(path,x+1,t);
        if(!(stat(t,finfo))) {
            if(S_ISREG(finfo->st_mode)) {
                strcpy(path_args,&path[strlen(t)]);
                strcpy(path,t);
                return;
            }
        }
    }
    unmunge_name(path);
    log_reason("script does not exist",path);
    die(NOT_FOUND,path,out);
}

#define MAX_COMMON_VARS 16
#define MAX_CGI_VARS (MAX_COMMON_VARS+16)

char **add_cgi_vars(char **env,
                    char *method, char *path, char *path_args, char *args,
                    int *content,
                    FILE *out)
{
    int x;
    char t[HUGE_STRING_LEN],t2[HUGE_STRING_LEN];

    if(!(env = new_env(env,MAX_CGI_VARS,&x)))
        die(NO_MEMORY,"add_cgi_vars",out);

    env[x++] = make_env_str("GATEWAY_INTERFACE","CGI/1.1",out);

    env[x++] = make_env_str("SERVER_PROTOCOL",
              (assbackwards ? "HTTP/0.9" : "HTTP/1.0"),out);
    env[x++] = make_env_str("REQUEST_METHOD",method,out);

    strcpy(t,path);
    unmunge_name(t);
    env[x++] = make_env_str("SCRIPT_NAME",t,out);
    if(path_args[0]) {
        env[x++] = make_env_str("PATH_INFO",path_args,out);
        strcpy(t2,path_args);
        translate_name(t2,out);
        env[x++] = make_env_str("PATH_TRANSLATED",t2,out);
    }
    env[x++] = make_env_str("QUERY_STRING",args,out);

    if(content) {
        *content=0;
        if((!strcmp(method,"POST")) || (!strcmp(method,"PUT"))) {
            *content=1;
            sprintf(t,"%d",content_length);
            env[x++] = make_env_str("CONTENT_TYPE",content_type,out);
            env[x++] = make_env_str("CONTENT_LENGTH",t,out);
        }
    }
    if (ErrorStat) {
      if (failed_request[0]) 
	env[x++] = make_env_str("REDIRECT_REQUEST",failed_request,out);
      if (failed_url[0])
	env[x++] = make_env_str("REDIRECT_URL",failed_url,out);
      env[x++] = make_env_str("REDIRECT_STATUS",set_stat_line(),out);
    }
    env[x] = NULL;
    return env;
}

char **add_common_vars(char **env,FILE *out) {
    char t[MAX_STRING_LEN],*env_path,*env_tz;
    int x;

    if(!(env = new_env(env,MAX_COMMON_VARS,&x)))
        die(NO_MEMORY,"add_common_vars",out);
    
    if(!(env_path = getenv("PATH")))
        env_path=DEFAULT_PATH;
    env[x++] = make_env_str("PATH",env_path,out);
    if((env_tz = getenv("TZ")))
	env[x++] = make_env_str("TZ",env_tz,out);
    env[x++] = make_env_str("SERVER_SOFTWARE",SERVER_VERSION,out);
    env[x++] = make_env_str("SERVER_NAME",server_hostname,out);
    sprintf(t,"%d",port);
    env[x++] = make_env_str("SERVER_PORT",t,out);
    env[x++] = make_env_str("REMOTE_HOST",remote_name,out);
    env[x++] = make_env_str("REMOTE_ADDR",remote_ip,out);
    env[x++] = make_env_str("DOCUMENT_ROOT",document_root,out);
    if(user[0])
        env[x++] = make_env_str("REMOTE_USER",user,out);
    if(annotation_server[0])
        env[x++] = make_env_str("ANNOTATION_SERVER",annotation_server,out);
    if(groupname[0])
	env[x++] = make_env_str("REMOTE_GROUP",groupname,out);
    if(auth_type)
        env[x++] = make_env_str("AUTH_TYPE",auth_type,out);
    if(do_rfc931)
        env[x++] = make_env_str("REMOTE_IDENT",remote_logname,out);
    if (ErrorStat) {
      if (failed_request[0]) 
        env[x++] = make_env_str("REDIRECT_REQUEST",failed_request,out);
      if (failed_url[0]) 
        env[x++] = make_env_str("REDIRECT_URL",failed_url,out);
      env[x++] = make_env_str("REDIRECT_STATUS",set_stat_line(),out);
    }
    env[x] = NULL;
    return env;
}

int cgi_stub(char *method, char *path, char *path_args, char *args,
             char **env, struct stat *finfo, int in, FILE *out)
{
    int p[2];
    int content, nph;
    char *argv0;
    FILE *psin;
    char errlog[100];

    if(!can_exec(finfo)) {
        unmunge_name(path);
        die(FORBIDDEN,path,out);
    }

    if((argv0 = strrchr(path,'/')) != NULL)
        argv0++;
    else argv0 = path;

    chdir_file(path);

    if(pipe(p) < 0)
        die(SERVER_ERROR,"httpd: could not create IPC pipe",out);
    if((pid = fork()) < 0) {
	sprintf(errlog,"httpd: could not fork new process: 2/%d",errno);
        die(SERVER_ERROR,errlog,out);
    }

    nph = (strncmp(argv0,"nph-",4) ? 0 : 1);
    if(!pid) {
        close(p[0]);
        in_headers_env = add_cgi_vars(in_headers_env,method,path,path_args,args,&content,out);
        if(content)
            if(in != STDIN_FILENO) {
                dup2(in,STDIN_FILENO);
                close(in);
            }
        if(nph) {
            if(fileno(out) != STDOUT_FILENO) {
                dup2(fileno(out),STDOUT_FILENO);
                fclose(out);
            }
        } else {
            if(p[1] != STDOUT_FILENO) {
                dup2(p[1],STDOUT_FILENO);
                close(p[1]);
            }
        }
        error_log2stderr();
/* To make the signal handling work on HPUX, according to 
   David-Michael Lincke (dlincke@bandon.unisg.ch) */
#ifdef HPUX
	signal(SIGCHLD, SIG_DFL);
#endif
        /* Only ISINDEX scripts get decoded arguments. */
        if((!args[0]) || (ind(args,'=') >= 0)) {
            if(execle(path,argv0,(char *)0,in_headers_env) == -1) {
                fprintf(stderr,"httpd: exec of %s failed, errno is %d\n",
                        path,errno);
                exit(1);
            }
        }
        else {
            if(execve(path,create_argv(argv0,args,out),in_headers_env) == -1) {
                fprintf(stderr,"httpd: exec of %s failed, errno is %d\n",
                        path,errno);
                exit(1);
            }
        }
    }
    else {
	if (nph) close(p[0]);
        close(p[1]);
    }

    if(!nph) {
        if(!(psin = fdopen(p[0],"r")))
            die(SERVER_ERROR,"could not read from script",out);

	content_type[0] = '\0';
        scan_script_header(psin,out);
        /* we never redirect and die now 
          if(scan_script_header(psin,out)) {
              kill_children(); 
              return REDIRECT_URL;
          }
        */


        if(location[0] == '/') {
            char t[HUGE_STRING_LEN],a[HUGE_STRING_LEN],*argp;

            a[0] = '\0';
            fclose(psin);
            waitpid(pid,NULL,0);
            strcpy(t,location);
            if(argp = strchr(t,'?')) {
                *argp++ = '\0';
                strcpy(a,argp);
            }
	    status = 302;
	    log_transaction();
	    status = 200;
            init_header_vars(); /* clear in_header_env and location */
	    sprintf(the_request,"GET ");
	    strncat(the_request,t,HUGE_STRING_LEN - strlen(the_request));
	    if (a[0] != '\0') {
		strncat(the_request,"?",HUGE_STRING_LEN - strlen(the_request));
		strncat(the_request,a, HUGE_STRING_LEN - strlen(the_request));
	    }
	    
	    strncat(the_request," ",HUGE_STRING_LEN - strlen(the_request));
	    strncat(the_request,protocal, HUGE_STRING_LEN - strlen(the_request));
            process_get(in,out,"GET",t,a);
            return REDIRECT_LOCAL;
        }
        content_length = -1;
        if(!assbackwards)
            send_http_header(out);
        if(!header_only) {
            /* Send a default body of text if the script
                failed to produce any, but ONLY for redirects */
            if (!send_fd(psin,out,NULL) && location[0]) {
                 title_html(out,"Document moved");
                 fprintf(out,"This document has moved <A HREF=\"%s\">here</A>.<P>%c",location,LF); 
            }
        } else
            kill_children();
        fclose(psin);
    }
    else bytes_sent = -1;
    waitpid(pid,NULL,0);
    return 0;
}

/* Called for ScriptAliased directories */
void exec_cgi_script(char *method, char *path, char *args, int in, FILE *out)
{
    struct stat finfo;
    char path_args[HUGE_STRING_LEN];
    int m = M_GET;
    int stub_returns;

    get_path_info(path,path_args,out,&finfo);
    if((!strcmp(method,"GET")) || (!strcmp(method,"HEAD"))) m=M_GET;
    else if(!strcmp(method,"POST")) m=M_POST;
    else if(!strcmp(method,"PUT")) m=M_PUT;
    else if(!strcmp(method,"DELETE")) m=M_DELETE;

    evaluate_access(path,&finfo,m,&allow,&allow_options,out);
    if(!allow) {
        log_reason("client denied by server configuration",path);
        unmunge_name(path);
        die(FORBIDDEN,path,out);
    }
    if(!(in_headers_env = add_common_vars(in_headers_env,out))) {
	free_env(in_headers_env);
	in_headers_env = NULL;
        die(NO_MEMORY,"exec_cgi_script",out);
    }

    bytes_sent = 0;
    stub_returns = cgi_stub(method,path,path_args,args,in_headers_env,&finfo,in,out);
    if (in_headers_env != NULL) { 
      free_env(in_headers_env);
      in_headers_env = NULL;
    }

    switch (stub_returns) {
	case REDIRECT_URL:
		die(REDIRECT,location,out);
		break;
	case REDIRECT_LOCAL:
		break;
	default:
		log_transaction();
		break;
    }
}

char **set_env_NCSA(FILE *out) {
    char **env;
    int n;
    char t[MAX_STRING_LEN];

    if(!(env = (char **) malloc ((4+2)*sizeof(char *))))
        die(NO_MEMORY,"set_env_NCSA",out);
    n=0;
    env[n++] = make_env_str("PATH",getenv("PATH"),out);

    env[n++] = make_env_str("DOCUMENT_ROOT",document_root,out);
    env[n++] = make_env_str("SERVER_ROOT",server_root,out);
    env[n++] = make_env_str("REMOTE_HOST",remote_name,out);
    sprintf(t,"SERVER_NAME=%s:%d",server_hostname,port);
    if(!(env[n++] = strdup(t)))
        die(NO_MEMORY,"set_env_NCSA",out);
    env[n] = NULL;
    return env;
}

void exec_get_NCSA(char *path, char *args, int in, FILE *fd) {
    FILE *tfp;
    struct stat finfo;
    int pfd[2];
    char path_args[MAX_STRING_LEN];
    char t[MAX_STRING_LEN];
    register int n,x;
    char **env;

    env = set_env_NCSA(fd);

    path_args[0] = '\0';
    /* check if it's really a script with extra args */
    n=count_dirs(path);
    for(x=0;x<=n;x++) {
        make_dirstr(path,x+1,t);
        if(!(stat(t,&finfo))) {
            if(S_ISREG(finfo.st_mode)) {
                strcpy(path_args,&path[strlen(t)]);
                strcpy(path,t);
                goto run_script;
            }
        }
    }
    log_reason("script not found or unable to stat",path);
    unmunge_name(path);
    die(NOT_FOUND,path,fd);
  run_script:
    if(!can_exec(&finfo)) {
        log_reason("file permissions deny server execution",path);
        unmunge_name(path);
        die(FORBIDDEN,path,fd);
    }
    evaluate_access(path,&finfo,M_GET,&allow,&allow_options,fd);
    if(!allow) {
        unmunge_name(path);
        die(FORBIDDEN,path,fd);
    }

    if(pipe(pfd) < 0)
        die(SERVER_ERROR,"could not open pipe",fd);

    signal(SIGALRM,send_fd_timed_out);
    signal(SIGPIPE,send_fd_timed_out);
    alarm(timeout);

    if((pid = fork()) < 0)
        die(SERVER_ERROR,"could not fork",fd);
    else if(!pid) {
        char *argv0;

        close(pfd[0]);
        if(pfd[1] != STDOUT_FILENO) {
            dup2(pfd[1],STDOUT_FILENO);
            close(pfd[1]);
        }
        if(argv0 = strrchr(path,'/'))
            argv0++;
        else
            argv0 = path;
        if(args[0] && path_args[0]) {
            if(execle(path,argv0,path_args,args,(char *)0,env) == -1)
                exit(1);
        }
        else if(args[0]) {
            if(execle(path,argv0,args,(char *)0,env) == -1)
                exit(1);
        }
        else if(path_args[0]) {
            if(execle(path,argv0,path_args,(char *)0,env) == -1)
                exit(1);
        }
        else
            if(execle(path,argv0,(char *)0,env) == -1)
                exit(1);
    }
    else
        close(pfd[1]);

    tfp = fdopen(pfd[0],"r");


    scan_script_header(tfp,fd);
    /*  don't force the redirect.. it'll happen
    if(scan_script_header(tfp,fd))
        die(REDIRECT,location,fd);
    */

    if(location[0] == '/') {
        char *t;
        if(!(t = strdup(location)))
            die(NO_MEMORY,"exec_get_NCSA",fd);
        location[0] = '\0';
        send_node(t,"",in,fd);
	fclose(tfp);
        htexit(0,fd);
    }

    if(!assbackwards)
        send_http_header(fd);

    if(!header_only) {
        /* Send a default body of text if the script
            failed to produce any, but ONLY for redirects */
        if (!send_fd(tfp,fd,NULL) &&  location[0]) {
           title_html(fd,"Document moved");
           fprintf(fd,"This document has moved <A HREF=\"%s\">here</A>.<P>%c",location,LF);
        }
    } else
        kill_children();
    fclose(tfp);
    waitpid(pid,NULL,0);
}



void exec_post_NCSA(char *path, char *args, int in, FILE *out) {
    int inpipe[2],outpipe[2];
    char cl[MAX_STRING_LEN];
    FILE *psin;
    struct stat finfo;
    char **env;
    char errlog[100];

    env = set_env_NCSA(out);

    sprintf(cl,"%d",content_length);

    if(stat(path,&finfo) == -1) {
        unmunge_name(path);
        if(errno == ENOENT) die(NOT_FOUND,path,out);
        die(FORBIDDEN,path,out);
    }
    evaluate_access(path,&finfo,M_POST,&allow,&allow_options,out);
    if(!allow)
        die(FORBIDDEN,path,out);

    if(pipe(inpipe) < 0)
        die(SERVER_ERROR,"httpd: could not create IPC pipe",out);
    if(pipe(outpipe) < 0)
        die(SERVER_ERROR,"httpd: could not create IPC pipe",out);
    if((pid = fork()) < 0) {
	sprintf(errlog,"httpd: could not fork new process 1/%d",errno);
        die(SERVER_ERROR,errlog,out);
    }
    if(!pid) {
        char *argv0;

        if(outpipe[1] != STDOUT_FILENO) {
            dup2(outpipe[1],STDOUT_FILENO);
            close(outpipe[1]);
        }
        if(in != STDIN_FILENO) {
            dup2(in,STDIN_FILENO);
            close(in);
        }
        if((argv0 = strrchr(path,'/')) != NULL)
            argv0++;
        else argv0 = path;
        if(execle(path,argv0,cl,args,(char *)0,env) == -1)
            exit(1);
    }
    else {
        close(outpipe[1]);
        close(inpipe[0]);
    }

    if(!(psin = fdopen(outpipe[0],"r")))
        die(SERVER_ERROR,"could not read from script",out);


    scan_script_header(psin,out);
    /* don't force the redirect, it'll happen 
    if(scan_script_header(psin,out))
        die(REDIRECT,location,out);
    */

    if(location[0] == '/') {
        char *t;
        if(!(t = strdup(location)))
            die(NO_MEMORY,"exec_post_NCSA",out);
        location[0] = '\0';
        send_node(t,"",in,out);
	fclose(psin);
        htexit(0,out);
    }

    content_length = -1;
    if(!assbackwards)
        send_http_header(out);

    /* send a default body of text if the script
       failed to produce any, but ONLY for redirects */
    if (!send_fd(psin,out,NULL) && location[0]) {
           title_html(out,"Document moved");
           fprintf(out,"This document has moved <A HREF=\"%s\">here</A>.<P>%c",location,LF);
    }
    fclose(psin);
    waitpid(pid,NULL,0);
}
