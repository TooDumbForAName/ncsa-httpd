/*
 * http_script: keeps all script-related ramblings together.
 * 
 * Compliant to CGI/1.0 spec
 * 
 * Rob McCool
 *
 */

#include "httpd.h"

char **create_argv(char *av0, char *args, FILE *out) {
    register int x,n;
    char **av;
    char w[MAX_STRING_LEN];
    char l[MAX_STRING_LEN];

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
    char t[MAX_STRING_LEN];

    path_args[0] = '\0';
    max=count_dirs(path);
    for(x=dirs_in_alias;x<=max;x++) {
        make_dirstr(path,x+1,t);
        if(!(stat(t,finfo))) {
            if(S_ISREG(finfo->st_mode)) {
                strcpy(path_args,&path[strlen(t)]);
                strcpy(path,t);
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

#define MAX_CGI_VARS 18

void exec_cgi_script(char *method, char *path, char *args, int in, FILE *out) 
{
    int pid, p[2];
    int content, nph;
    char cl[MAX_STRING_LEN],t[MAX_STRING_LEN],t2[MAX_STRING_LEN];
    char path_args[MAX_STRING_LEN];
    char *argv0,**env;
    FILE *psin;
    struct stat finfo;
    register int n,x;

    get_path_info(path,path_args,out,&finfo);
    
    if(!can_exec(&finfo)) {
        unmunge_name(path);
        die(FORBIDDEN,path,out);
    }

    /* BAD -- method specific */
    evaluate_access(path,&finfo,((!strcmp(method,"POST")) ? M_POST : M_GET),
                    &allow,&allow_options,out);
    if(!allow) {
        log_reason("client denied by server configuration",path);
        unmunge_name(path);
        die(FORBIDDEN,path,out);
    }

    if(!(env = (char **)malloc((MAX_CGI_VARS + 2) * sizeof(char *))))
        die(NO_MEMORY,"exec_cgi_script",out);
    n = 0;
    env[n++] = make_env_str("PATH",getenv("PATH"),out);
    env[n++] = make_env_str("SERVER_SOFTWARE",SERVER_VERSION,out);
    env[n++] = make_env_str("SERVER_NAME",server_hostname,out);
    env[n++] = make_env_str("GATEWAY_INTERFACE","CGI/1.0",out);

    sprintf(t,"%d",port);
    env[n++] = make_env_str("SERVER_PORT",t,out);

    env[n++] = make_env_str("SERVER_PROTOCOL",
              (assbackwards ? "HTTP/0.9" : "HTTP/1.0"),out);
    env[n++] = make_env_str("REQUEST_METHOD",method,out);
    env[n++] = make_env_str("HTTP_ACCEPT",http_accept,out);
    if(path_args[0]) {
        env[n++] = make_env_str("PATH_INFO",path_args,out);
        strcpy(t2,path_args);
        translate_name(t2,out);
        env[n++] = make_env_str("PATH_TRANSLATED",t2,out);
    }
    strcpy(t,path);
    unmunge_name(t);
    env[n++] = make_env_str("SCRIPT_NAME",t,out);
    env[n++] = make_env_str("QUERY_STRING",args,out);
    env[n++] = make_env_str("REMOTE_HOST",remote_name,out);
    env[n++] = make_env_str("REMOTE_ADDR",remote_ip,out);
    if(user[0])
        env[n++] = make_env_str("REMOTE_USER",user,out);
    if(auth_type)
        env[n++] = make_env_str("AUTH_TYPE",auth_type,out);

    if(do_rfc931)
        env[n++] = make_env_str("REMOTE_IDENT",remote_logname,out);
    content=0;
    if((!strcmp(method,"POST")) || (!strcmp(method,"PUT"))) {
        content=1;
        sprintf(cl,"%d",content_length);
        env[n++] = make_env_str("CONTENT_TYPE",content_type,out);
        env[n++] = make_env_str("CONTENT_LENGTH",cl,out);
    }
    env[n] = NULL;

    if((argv0 = strrchr(path,'/')) != NULL)
        argv0++;
    else argv0 = path;

    if(pipe(p) < 0)
        die(SERVER_ERROR,"httpd: could not create IPC pipe",out);
    if((pid = fork()) < 0)
        die(SERVER_ERROR,"httpd: could not fork new process",out);

    nph = (strncmp(argv0,"nph-",4) ? 0 : 1);
    if(!pid) {
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
        for(x=0;x<n;x++)
            free(env[n]);
        free(env);
        close(p[1]);
    }

    if(!nph) {
        if(!(psin = fdopen(p[0],"r")))
            die(SERVER_ERROR,"could not read from script",out);

        if(scan_script_header(psin,out))
            die(REDIRECT,location,out);

        if(location[0] == '/') {
            char t[MAX_STRING_LEN],a[MAX_STRING_LEN],*argp;

            a[0] = '\0';
            fclose(psin);
            waitpid(pid,NULL,0);
            strcpy(t,location);
            location[0] = '\0';
            if(argp = strchr(t,'?')) {
                *argp++ = '\0';
                strcpy(a,argp);
            }
            process_get(in,out,method,t,a);
            return;
        }
        content_length = -1;
        if(!assbackwards)
            send_http_header(out);
        if(!header_only)
            send_fd(psin,out,args);
        /* This will cause SIGPIPE in child... it should terminate */
        fclose(psin);
    }
    waitpid(pid,NULL,0);
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

void exec_get_NCSA(char *path, char *args, FILE *fd) {
    FILE *tfp;
    struct stat finfo;
    int pid,pfd[2];
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
        send_node(t,"",fd);
        htexit(0,fd);
    }

    if(!assbackwards)
        send_http_header(fd);

    if(!header_only)
        send_fd(tfp,fd,args);
    fclose(tfp);
    waitpid(pid,NULL,0);
}



void exec_post_NCSA(char *path, char *args, int in, FILE *out) {
    int pid, inpipe[2],outpipe[2], x;
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
        send_node(t,"",out);
        htexit(0,out);
    }

    content_length = -1;
    if(!assbackwards)
        send_http_header(out);

    send_fd(psin,out,args);
    fclose(psin);
    waitpid(pid,NULL,0);
}
