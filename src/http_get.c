/*
 * http_get.c: Handles things associated with GET
 * 
 * Rob McCool
 * 
 */

#include "httpd.h"


int header_only;
int num_includes;
int allow;
char allow_options;


void send_file(char *file, FILE *fd, struct stat *fi, 
               char *path_args, char *args) 
{
    FILE *f;

    set_content_type(file);

    if((allow_options & OPT_INCLUDES) && (!content_encoding[0])) {
#ifdef XBITHACK
        if((fi->st_mode & S_IXUSR) ||
           (!strcmp(content_type,INCLUDES_MAGIC_TYPE))) {
#else
        if(!strcmp(content_type,INCLUDES_MAGIC_TYPE)) {
#endif
            status = 200;
            bytes_sent = 0;
            send_parsed_file(file,fd,path_args,args,
                             allow_options & OPT_INCNOEXEC);
            log_transaction();
            return;
        }
    }
    if(!(f=fopen(file,"r"))) {
        log_reason("file permissions deny server access",file);
        unmunge_name(file);
        die(FORBIDDEN,file,fd); /* we've already established that it exists */
    }
    status = 200;
    bytes_sent = 0;
    if(!assbackwards) {
        set_content_length(fi->st_size);
        set_last_modified(fi->st_mtime,fd);
        send_http_header(fd);
    }
    num_includes=0;
    if(!header_only) 
        send_fd(f,fd,NULL);
    log_transaction();
    fclose(f);
}

void send_cgi(char *method, char *file, char *path_args, char *args, 
              struct stat *finfo, int in, FILE *fd) 
{
    char **env;
    int m;

    if((!strcmp(method,"GET")) || (!strcmp(method,"HEAD")))
        m = M_GET;
    else if(!strcmp(method,"POST"))
        m = M_POST;
    else if(!strcmp(method,"PUT"))
        m = M_PUT;
    else if(!strcmp(method,"DELETE"))
        m = M_DELETE;

    evaluate_access(file,finfo,m,&allow,&allow_options,fd);
    if((!allow) || (!(allow_options & OPT_EXECCGI))) {
        log_reason("client denied by server configuration",file);
        unmunge_name(file);
        die(FORBIDDEN,file,fd);
    }
    if(!(env = add_common_vars(in_headers_env,fd)))
        die(NO_MEMORY,"send_cgi",fd);
    bytes_sent = 0;
    if(cgi_stub(method,file,path_args,args,env,finfo,in,fd) == REDIRECT_URL)
        die(REDIRECT,location,fd);
    free_env(env);
    log_transaction();
}

void send_node(char *file, char *args, int in, FILE *fd)
{
    struct stat finfo;
    char pa[MAX_STRING_LEN];

    pa[0] = '\0';
    if(stat(file,&finfo) == -1) {
        /* Look for script or include document */
        int n=count_dirs(file),i,l;
        char t[MAX_STRING_LEN];
        
        for(i=n;i;--i) {
            make_dirstr(file,i,t);
            probe_content_type(t);
            if(!strcmp(content_type,CGI_MAGIC_TYPE)) {
                if(stat(t,&finfo) == -1)
                    continue;
                if(!(S_ISREG(finfo.st_mode)))
                    break;
                l=strlen(t);
                strcpy(pa,&file[l]);
                file[l] = '\0';
                send_cgi("GET",file,pa,args,&finfo,in,fd);
                return;
            } else if(!strcmp(content_type,INCLUDES_MAGIC_TYPE)) {
                if(stat(t,&finfo) == -1)
                    continue;
                l=strlen(t);
                strcpy(pa,&file[l]);
                file[l] = '\0';
                goto send_regular;
            }
        }
        if(errno==ENOENT) {
            log_reason("file does not exist",file);
            unmunge_name(file);
            die(NOT_FOUND,file,fd);
        }
        else {
            log_reason("file permissions deny server access",file);
            unmunge_name(file);
            die(FORBIDDEN,file,fd);
        }
    }
    probe_content_type(file);
    if(S_ISREG(finfo.st_mode) && (!strcmp(content_type,CGI_MAGIC_TYPE))) {
        send_cgi("GET",file,"",args,&finfo,in,fd);
        return;
    }

  send_regular: /* aaaaack */
    evaluate_access(file,&finfo,M_GET,&allow,&allow_options, fd);
    if(!allow) {
        log_reason("client denied by server configuration",file);
        unmunge_name(file);
        die(FORBIDDEN,file,fd);
    }

    if(S_ISDIR(finfo.st_mode)) {
        char ifile[MAX_STRING_LEN];

        if(file[strlen(file) - 1] != '/') {
            char url[MAX_STRING_LEN];
            strcpy_dir(ifile,file);
            unmunge_name(ifile);
            construct_url(url,ifile);
            escape_url(url);
            die(REDIRECT,url,fd);
        }
        make_full_path(file,index_name,ifile);
        if(stat(ifile,&finfo) == -1) {
            if(allow_options & OPT_INDEXES)
                index_directory(file,fd);
            else {
                log_reason("file permissions deny server access",file);
                unmunge_name(file);
                die(FORBIDDEN,file,fd);
            }
        }
        else {
            probe_content_type(ifile);
            if(!strcmp(content_type,CGI_MAGIC_TYPE))
                send_cgi("GET",ifile,pa,args,&finfo,in,fd);
            else
                send_file(ifile,fd,&finfo,pa,args);
        }
        return;
    }
    if(S_ISREG(finfo.st_mode))
        send_file(file,fd,&finfo,pa,args);
    else {
        log_reason("improper file type",file);
        unmunge_name(file);
        die(FORBIDDEN,file,fd); /* device driver or pipe, no permission */
    }
}

void process_get(int in, FILE *out, char *m, char *url, char *args) {
    int s;

    if(assbackwards && header_only) {
        header_only = 0;
        die(BAD_REQUEST,"Invalid HTTP/0.9 method.",out);
    }
    s=translate_name(url,out);
    switch(s) {
      case STD_DOCUMENT:
        send_node(url,args,in,out);
        return;
      case REDIRECT_URL:
        die(REDIRECT,url,out);
      case SCRIPT_NCSA:
        exec_get_NCSA(url,args,in,out);
        return;
      case SCRIPT_CGI:
        exec_cgi_script(m,url,args,in,out);
        return;
    }
}
