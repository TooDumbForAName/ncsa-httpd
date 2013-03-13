/*
 * http_script: keeps all script-related ramblings together.
 * 
 * Compliant to CGI/1.0 spec
 * 
 * Rob McCool
 *
 */

#include "httpd.h"

int pid;

void kill_children() {
    char errstr[MAX_STRING_LEN];
    sprintf(errstr,"killing CGI process %d",pid);
    log_error_noclose(errstr);

    kill(pid,SIGTERM);
    sleep(5); /* give them time to clean up */
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
    die(NOT_FOUND,path,out);
}

#define MAX_COMMON_VARS 9
#define MAX_CGI_VARS (MAX_COMMON_VARS+9)

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
    env[x] = NULL;
    return env;
}

char **add_common_vars(char **env,FILE *out) {
    char t[HUGE_STRING_LEN],*env_path;
    int x;

    if(!(env = new_env(env,MAX_COMMON_VARS,&x)))
        die(NO_MEMORY,"add_common_vars",out);
    
    if(!(env_path = getenv("PATH")))
        env_path=DEFAULT_PATH;
    env[x++] = make_env_str("PATH",env_path,out);
    env[x++] = make_env_str("SERVER_SOFTWARE",SERVER_VERSION,out);
    env[x++] = make_env_str("SERVER_NAME",server_hostname,out);
    sprintf(t,"%d",port);
    env[x++] = make_env_str("SERVER_PORT",t,out);
    env[x++] = make_env_str("REMOTE_HOST",remote_name,out);
    env[x++] = make_env_str("REMOTE_ADDR",remote_ip,out);
    if(user[0])
        env[x++] = make_env_str("REMOTE_USER",user,out);
    if(auth_type)
        env[x++] = make_env_str("AUTH_TYPE",auth_type,out);
    if(do_rfc931)
        env[x++] = make_env_str("REMOTE_IDENT",remote_logname,out);
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
    register int x;

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
    if((pid = fork()) < 0)
        die(SERVER_ERROR,"httpd: could not fork new process",out);

    nph = (strncmp(argv0,"nph-",4) ? 0 : 1);
    if(!pid) {
        close(p[0]);
        env = add_cgi_vars(env,method,path,path_args,args,&content,out);
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
        /* Only ISINDEX scripts get decoded arguments. */
        if((!args[0]) || (ind(args,'=') >= 0)) {
            if(execle(path,argv0,(char *)0,env) == -1) {
                fprintf(stderr,"httpd: exec of %s failed, errno is %d\n",
                        path,errno);
                exit(1);
            }
        }
        else {
            if(execve(path,create_argv(argv0,args,out),env) == -1) {
                fprintf(stderr,"httpd: exec of %s failed, errno is %d\n",
                        path,errno);
                exit(1);
            }
        }
    }
    else {
        close(p[1]);
    }

    if(!nph) {
        if(!(psin = fdopen(p[0],"r")))
            die(SERVER_ERROR,"could not read from script",out);

        if(scan_script_header(psin,out)) {
            kill_children(); /* !!! */
            return REDIRECT_URL;
        }

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
            init_header_vars(); /* clear in_header_env and location */
            process_get(in,out,"GET",t,a);
            return 0;
        }
        content_length = -1;
        if(!assbackwards)
            send_http_header(out);
        if(!header_only)
            send_fd(psin,out,kill_children);
        else
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
    char **env;
    int m;

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
    if(!(env = add_common_vars(in_headers_env,out)))
        die(NO_MEMORY,"exec_cgi_script",out);

    bytes_sent = 0;
    if(cgi_stub(method,path,path_args,args,env,&finfo,in,out) == REDIRECT_URL)
        die(REDIRECT,location,out);
    /* cgi_stub will screw with env, but only after the fork */
    free_env(env);
    log_transaction();
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

    if(scan_script_header(tfp,fd))
        die(REDIRECT,location,fd);

    if(location[0] == '/') {
        char *t;
        if(!(t = strdup(location)))
            die(NO_MEMORY,"exec_get_NCSA",fd);
        location[0] = '\0';
        send_node(t,"",in,fd);
        htexit(0,fd);
    }

    if(!assbackwards)
        send_http_header(fd);

    if(!header_only)
        send_fd(tfp,fd,kill_children);
    else
        kill_children();
    fclose(tfp);
    waitpid(pid,NULL,0);
}



void exec_post_NCSA(char *path, char *args, int in, FILE *out) {
    int inpipe[2],outpipe[2], x;
    char cl[MAX_STRING_LEN];
    FILE *psin,*psout;
    struct stat finfo;
    char **env;
    
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
    if((pid = fork()) < 0)
        die(SERVER_ERROR,"httpd: could not fork new process",out);

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

    if(scan_script_header(psin,out))
        die(REDIRECT,location,out);

    if(location[0] == '/') {
        char *t;
        if(!(t = strdup(location)))
            die(NO_MEMORY,"exec_post_NCSA",out);
        location[0] = '\0';
        send_node(t,"",in,out);
        htexit(0,out);
    }

    content_length = -1;
    if(!assbackwards)
        send_http_header(out);

    send_fd(psin,out,kill_children);
    fclose(psin);
    waitpid(pid,NULL,0);
}
