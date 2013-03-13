/*
 * http_include.c: Handles the server-parsed HTML documents
 * 
 * Rob McCool
 * 
 */

#include "httpd.h"

#define STARTING_SEQUENCE "<!--#"
#define ENDING_SEQUENCE "-->"
#define DEFAULT_ERROR_MSG "[an error occurred while processing this directive]"
#define DEFAULT_TIME_FORMAT "%A, %d-%b-%y %T %Z"
#define SIZEFMT_BYTES 0
#define SIZEFMT_KMG 1

/* These are stored statically so that they can be reformatted quickly */
static time_t date,lm;

/* ------------------------ Environment function -------------------------- */

#define NUM_INCLUDE_VARS 5

char **add_include_vars(char **env,char *file, char *path_args, char *args, 
                        char *timefmt,FILE *out)
{
    int x;
    struct stat finfo;
    char ufile[HUGE_STRING_LEN];
    char *t;

    if(!(env = new_env(env,NUM_INCLUDE_VARS,&x)))
        die(NO_MEMORY,"add_include_vars",out);
    date = time(NULL);
    env[x++] = make_env_str("DATE_LOCAL",ht_time(date,timefmt,0),out);
    env[x++] = make_env_str("DATE_GMT",ht_time(date,timefmt,1),out);

    if(stat(file,&finfo) != -1) {
        lm = finfo.st_mtime;
        env[x++] = make_env_str("LAST_MODIFIED",ht_time(lm,timefmt,0),out);
    }
    strcpy(ufile,file);
    unmunge_name(ufile);
    env[x++] = make_env_str("DOCUMENT_URI",ufile,out);
    if(t = strrchr(ufile,'/'))
        ++t;
    else
        t = ufile;
    env[x++] = make_env_str("DOCUMENT_NAME",t,out);
    env[x] = NULL;
    return env;
}

#define GET_CHAR(f,c,r) \
 { \
   int i = getc(f); \
   if(feof(f) || ferror(f) || (i == -1)) { \
        fclose(f); \
        return r; \
   } \
   c = (char)i; \
 }

/* --------------------------- Parser functions --------------------------- */

int find_string(FILE *in,char *str, FILE *out) {
    int x,l=strlen(str),p;
    char c;

    p=0;
    while(1) {
        GET_CHAR(in,c,1);
        if(c == str[p]) {
            if((++p) == l)
                return 0;
        }
        else {
            if(out) {
                if(p) {
                    for(x=0;x<p;x++) {
                        putc(str[x],out);
                        ++bytes_sent;
                    }
                }
                putc(c,out);
                ++bytes_sent;
            }
            p=0;
        }
    }
}

char *get_tag(FILE *in, char *tag) {
    char *t = tag, *tag_val, c;
    int n;

    n = 0;
    while(1) {
        GET_CHAR(in,c,NULL);
        if(!isspace(c)) break;
    }
    /* problem: this drops tags starting with - or -- (tough s***) */
    if(c == '-') {
        GET_CHAR(in,c,NULL);
        if(c == '-') {
            GET_CHAR(in,c,NULL);
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
            GET_CHAR(in,c,NULL);
            *t = c;
        } else if(*t == '=') {
            *t++ = '\0';
            tag_val = t;
            GET_CHAR(in,c,NULL);
            if(c == '\"') {
                while(1) {
                    GET_CHAR(in,c,NULL);
                    if(++n == MAX_STRING_LEN) {
                        t[MAX_STRING_LEN - 1] = '\0';
                        return NULL;
                    }
                    if((*t = c) == '\\') {
                        GET_CHAR(in,c,NULL);
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
        GET_CHAR(in,c,NULL);
    }
}

int get_directive(FILE *in,char *d) {
    char c;

    /* skip initial whitespace */
    while(1) {
        GET_CHAR(in,c,1);
        if(!isspace(c))
            break;
    }
    /* now get directive */
    while(1) {
        *d++ = c;
        GET_CHAR(in,c,1);
        if(isspace(c))
            break;
    }
    *d = '\0';
    return 0;
}

/* --------------------------- Action handlers ---------------------------- */


void send_parsed_content(char *file, FILE *f, FILE *fd, 
                         char *path_args, char *args,
                         char **env,int noexec);

int send_included_file(char *file, FILE *out, char **env, char *fn) 
{
    FILE *f;
    struct stat finfo;
    int allow;char op,i;

    if(stat(file,&finfo) == -1)
        return -1;
    evaluate_access(file,&finfo,M_GET,&allow,&op,out);
    if(!allow)
        return -1;
    set_content_type(file);
    if((op & OPT_INCLUDES) && (!strcmp(content_type,INCLUDES_MAGIC_TYPE))) {
        if(!(f = fopen(file,"r")))
            return -1;
        send_parsed_content(file,f,out,"","",env,op & OPT_INCNOEXEC);
        chdir_file(fn); /* grumble */
    }
    else if(!strcmp(content_type,CGI_MAGIC_TYPE))
        return -1;
    else {
        if(!(f=fopen(file,"r")))
            return -1;
        send_fd(f,out,NULL);
        fclose(f);
    }
    return 0;
}

int handle_include(FILE *in, FILE *out, char *fn, char **env, char *error) {
    char tag[MAX_STRING_LEN],errstr[MAX_STRING_LEN];
    char *tag_val;

    while(1) {
        if(!(tag_val = get_tag(in,tag)))
            return 1;
        if(!strcmp(tag,"file")) {
            char dir[MAX_STRING_LEN],to_send[MAX_STRING_LEN];

            getparents(tag_val); /* get rid of any nasties */
            getwd(dir);
            make_full_path(dir,tag_val,to_send);
            if(send_included_file(to_send,out,env,fn)) {
                sprintf(errstr,"unable to include %s in parsed file %s",
                        tag_val, fn);
                log_error_noclose(errstr);
                bytes_sent += fprintf(out,"%s",error);
            }            
        } 
        else if(!strcmp(tag,"virtual")) {
            if(translate_name(tag_val,out) != STD_DOCUMENT) {
                bytes_sent += fprintf(out,"%s",error);
                log_error_noclose(errstr);
            }  
            else if(send_included_file(tag_val,out,env,fn)) {
                sprintf(errstr,"unable to include %s in parsed file %s",
                        tag_val, fn);
                log_error_noclose(errstr);
                bytes_sent += fprintf(out,"%s",error);
            }
        } 
        else if(!strcmp(tag,"done"))
            return 0;
        else {
            sprintf(errstr,"unknown parameter %s to tag echo in %s",tag,fn);
            log_error_noclose(errstr);
            bytes_sent += fprintf(out,"%s",error);
        }
    }
}

int handle_echo(FILE *in, FILE *out, char *file, char *error, char **env) {
    char tag[MAX_STRING_LEN];
    char *tag_val;

    while(1) {
        if(!(tag_val = get_tag(in,tag)))
            return 1;
        if(!strcmp(tag,"var")) {
            int x,i;

            for(x=0;env[x] != NULL; x++) {
                i = ind(env[x],'=');
                if(!strncmp(env[x],tag_val,i)) {
                    bytes_sent += fprintf(out,"%s",&env[x][i+1]);
                    break;
                }
            }
            if(!env[x]) bytes_sent += fprintf(out,"(none)");
        } else if(!strcmp(tag,"done"))
            return 0;
        else {
            char errstr[MAX_STRING_LEN];
            sprintf(errstr,"unknown parameter %s to tag echo in %s",tag,file);
            log_error_noclose(errstr);
            bytes_sent += fprintf(out,"%s",error);
        }
    }
}

int include_cgi(char *s, char *pargs, char *args, char **env, FILE *out) 
{
    char *argp,op,d[HUGE_STRING_LEN];
    int allow,check_cgiopt;
    struct stat finfo;

    getparents(s);
    if(s[0] == '/') {
        strcpy(d,s);
        if(translate_name(d,out) != SCRIPT_CGI)
            return -1;
        check_cgiopt=0;
    } else {
        char dir[MAX_STRING_LEN];
        getwd(dir);
        make_full_path(dir,s,d);
        check_cgiopt=1;
    }
    /* No hardwired path info or query allowed */
    if(stat(d,&finfo) == -1)
        return -1;

    evaluate_access(d,&finfo,M_GET,&allow,&op,out);
    if((!allow) || (check_cgiopt && (!(op & OPT_EXECCGI))))
        return -1;

    if(cgi_stub("GET",d,pargs,args,env,&finfo,-1,out) == REDIRECT_URL)
        bytes_sent += fprintf(out,"<A HREF=\"%s\">%s</A>",location,location);
    return 0;
}

static int ipid;
void kill_include_child() {
    char errstr[MAX_STRING_LEN];
    sprintf(errstr,"killing command process %d",ipid);
    log_error_noclose(errstr);
    kill(ipid,SIGKILL);
    waitpid(ipid,NULL,0);
}

int include_cmd(char *s, char *pargs, char *args, char **env, FILE *out) {
    int p[2],x;
    FILE *f;

    if(pipe(p) == -1)
        die(SERVER_ERROR,"httpd: could not create IPC pipe",out);
    if((ipid = fork()) == -1)
        die(SERVER_ERROR,"httpd: could not fork new process",out);
    if(!ipid) {
        char *argv0;

        if(pargs[0] || args[0]) {
            if(!(env = new_env(env,4,&x)))
                return -1;
            if(pargs[0]) {
                char p2[HUGE_STRING_LEN];
                
                escape_shell_cmd(pargs);
                env[x++] = make_env_str("PATH_INFO",pargs,out);
                strcpy(p2,pargs);
                translate_name(p2,out);
                env[x++] = make_env_str("PATH_TRANSLATED",p2,out);
            }
            if(args[0]) {
                env[x++] = make_env_str("QUERY_STRING",args,out);
                unescape_url(args);
                escape_shell_cmd(args);
                env[x++] = make_env_str("QUERY_STRING_UNESCAPED",args,out);
            }
            env[x] = NULL;
        }

        close(p[0]);
        if(p[1] != STDOUT_FILENO) {
            dup2(p[1],STDOUT_FILENO);
            close(p[1]);
        }
        error_log2stderr();
        if(!(argv0 = strrchr(SHELL_PATH,'/')))
            argv0=SHELL_PATH;
        if(execle(SHELL_PATH,argv0,"-c",s,(char *)0,env) == -1) {
            fprintf(stderr,"httpd: exec of %s failed, errno is %d\n",
                    SHELL_PATH,errno);
            exit(1);
        }
    }
    close(p[1]);
    if(!(f=fdopen(p[0],"r"))) {
        waitpid(ipid,NULL,0);
        return -1;
    }
    send_fd(f,out,kill_include_child);
    fclose(f);
    waitpid(ipid,NULL,0);
    return 0;
}


int handle_exec(FILE *in, FILE *out, char *file, char *path_args, char *args,
                char *error, char **env)
{
    char tag[MAX_STRING_LEN],errstr[MAX_STRING_LEN];
    char *tag_val;

    while(1) {
        if(!(tag_val = get_tag(in,tag)))
            return 1;
        if(!strcmp(tag,"cmd")) {
            if(include_cmd(tag_val,path_args,args,env,out) == -1) {
                sprintf(errstr,"invalid command exec %s in %s",tag_val,file);
                log_error_noclose(errstr);
                bytes_sent += fprintf(out,"%s",error);
            }
            /* just in case some stooge changed directories */
            chdir_file(file);
        } 
        else if(!strcmp(tag,"cgi")) {
            if(include_cgi(tag_val,path_args,args,env,out) == -1) {
                sprintf(errstr,"invalid CGI ref %s in %s",tag_val,file);
                log_error_noclose(errstr);
                bytes_sent += fprintf(out,"%s",error);
            }
            /* grumble groan */
            chdir_file(file);
        }
        else if(!strcmp(tag,"done"))
            return 0;
        else {
            char errstr[MAX_STRING_LEN];
            sprintf(errstr,"unknown parameter %s to tag echo in %s",tag,file);
            log_error_noclose(errstr);
            bytes_sent += fprintf(out,"%s",error);
        }
    }

}

int handle_config(FILE *in, FILE *out, char *file, char *error, char *tf,
                  int *sizefmt, char **env) {
    char tag[MAX_STRING_LEN];
    char *tag_val;

    while(1) {
        if(!(tag_val = get_tag(in,tag)))
            return 1;
        if(!strcmp(tag,"errmsg"))
            strcpy(error,tag_val);
        else if(!strcmp(tag,"timefmt")) {
            strcpy(tf,tag_val);
            /* Replace DATE* and LAST_MODIFIED (they should be first) */
            free(env[0]);
            env[0] = make_env_str("DATE_LOCAL",ht_time(date,tf,0),out);
            free(env[1]);
            env[1] = make_env_str("DATE_GMT",ht_time(date,tf,1),out);
            if(!strncmp(env[2],"LAST_MODIFIED",13)) {
                free(env[2]);
                env[2] = make_env_str("LAST_MODIFIED",ht_time(lm,tf,0),out);
            }
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
                    tag,file);
            log_error_noclose(errstr);
            bytes_sent += fprintf(out,"%s",error);
        }
    }
}



int find_file(FILE *out, char *file, char *directive, char *tag, 
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
                    to_send,file);
            log_error_noclose(errstr);
            bytes_sent += fprintf(out,"%s",error);
            return -1;
        }
        return 0;
    }
    else if(!strcmp(tag,"virtual")) {
        if(translate_name(tag_val,out) != STD_DOCUMENT) {
            bytes_sent += fprintf(out,"%s",error);
            log_error_noclose(errstr);
        }  
        else if(stat(tag_val,finfo) == -1) {
            sprintf(errstr,
                    "unable to get information about %s in parsed file %s",
                    to_send,file);
            log_error_noclose(errstr);
            bytes_sent += fprintf(out,"%s",error);
            return -1;
        }
        return 0;
    }
    else {
        sprintf(errstr,"unknown parameter %s to tag %s in %s",
                tag,directive,file);
        log_error_noclose(errstr);
        bytes_sent += fprintf(out,"%s",error);
        return -1;
    }
}


int handle_fsize(FILE *in, FILE *out, char *file, char *error, int sizefmt,
                 char **env) 
{
    char tag[MAX_STRING_LEN];
    char *tag_val;
    struct stat finfo;

    while(1) {
        if(!(tag_val = get_tag(in,tag)))
            return 1;
        else if(!strcmp(tag,"done"))
            return 0;
        else if(!find_file(out,file,"fsize",tag,tag_val,&finfo,error)) {
            if(sizefmt == SIZEFMT_KMG) {
                send_size(finfo.st_size,out);
                bytes_sent += 5;
            }
            else {
                int l,x;
                sprintf(tag,"%ld",finfo.st_size);
                l = strlen(tag); /* grrr */
                for(x=0;x<l;x++) {
                    if(x && (!((l-x) % 3))) {
                        fputc(',',out);
                        ++bytes_sent;
                    }
                    fputc(tag[x],out);
                    ++bytes_sent;
                }
            }
        }
    }
}

int handle_flastmod(FILE *in, FILE *out, char *file, char *error, char *tf,
                    char **env) 
{
    char tag[MAX_STRING_LEN];
    char *tag_val;
    struct stat finfo;

    while(1) {
        if(!(tag_val = get_tag(in,tag)))
            return 1;
        else if(!strcmp(tag,"done"))
            return 0;
        else if(!find_file(out,file,"flastmod",tag,tag_val,&finfo,error))
            bytes_sent += fprintf(out,"%s",ht_time(finfo.st_mtime,tf,0));
    }
}    



/* -------------------------- The main function --------------------------- */

/* This is a stub which parses a file descriptor. */

void send_parsed_content(char *file, FILE *f, FILE *fd, 
                         char *path_args, char *args,
                         char **env,int noexec)
{
    char directive[MAX_STRING_LEN], error[MAX_STRING_LEN], c;
    char timefmt[MAX_STRING_LEN], errstr[MAX_STRING_LEN];
    int ret, sizefmt;

    strcpy(error,DEFAULT_ERROR_MSG);
    strcpy(timefmt,DEFAULT_TIME_FORMAT);
    sizefmt = SIZEFMT_KMG;

    chdir_file(file);

    while(1) {
        if(!find_string(f,STARTING_SEQUENCE,fd)) {
            if(get_directive(f,directive))
                return;
            if(!strcmp(directive,"exec")) {
                if(noexec) {
                    sprintf(errstr,"httpd: exec used but not allowed in %s",
                            file);
                    log_error_noclose(errstr);
                    bytes_sent += fprintf(fd,"%s",error);
                    ret = find_string(f,ENDING_SEQUENCE,NULL);
                } else 
                    ret=handle_exec(f,fd,file,path_args,args,error,env);
            } 
            else if(!strcmp(directive,"config"))
                ret=handle_config(f,fd,file,error,timefmt,&sizefmt,env);
            else if(!strcmp(directive,"include"))
                ret=handle_include(f,fd,file,env,error);
            else if(!strcmp(directive,"echo"))
                ret=handle_echo(f,fd,file,error,env);
            else if(!strcmp(directive,"fsize"))
                ret=handle_fsize(f,fd,file,error,sizefmt,env);
            else if(!strcmp(directive,"flastmod"))
                ret=handle_flastmod(f,fd,file,error,timefmt,env);
            else {
                sprintf(errstr,"httpd: unknown directive %s in parsed doc %s",
                        directive,file);
                log_error_noclose(errstr);
                bytes_sent += fprintf(fd,"%s",error);
                ret=find_string(f,ENDING_SEQUENCE,NULL);
            }
            if(ret) {
                sprintf(errstr,"httpd: premature EOF in parsed file %s",file);
                log_error_noclose(errstr);
                return;
            }
        } else 
            return;
    }
}

/* Called by send_file */

void send_parsed_file(char *file, FILE *fd, char *path_args, char *args,
                      int noexec) 
{
    FILE *f;
    char **env;

    if(!(f=fopen(file,"r"))) {
        log_reason("file permissions deny server access",file);
        unmunge_name(file);
        die(FORBIDDEN,file,fd);
    }
    strcpy(content_type,"text/html");
    if(!assbackwards)
        send_http_header(fd);
    if(header_only)
        return;

    /* Make sure no children inherit our buffers */
    fflush(fd);
    assbackwards = 1; /* make sure no headers get inserted anymore */
    alarm(timeout);

    env = add_include_vars(in_headers_env,file,path_args,args,
                           DEFAULT_TIME_FORMAT,fd);
    env = add_common_vars(env,fd);

    send_parsed_content(file,f,fd,path_args,args,env,noexec);
    free_env(env);
}
