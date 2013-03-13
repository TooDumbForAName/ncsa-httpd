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


void send_file(char *file, FILE *fd, char *args) {
    FILE *f;
    struct stat finfo;

    if(!(f=fopen(file,"r"))) {
        log_reason("file permissions deny server access",file);
        unmunge_name(file);
        die(FORBIDDEN,file,fd); /* we've already established that it exists */
    }
    set_content_type(file);
    if(!assbackwards) {
        fstat(fileno(f),&finfo);
        if(!((allow_options & OPT_INCLUDES) && is_content_type("text/html")))
            set_content_length(finfo.st_size);
        set_last_modified(finfo.st_mtime);
        send_http_header(fd);
    }
    num_includes=0;
    if(!header_only) 
        send_fd(f,fd,args);
}


void send_node(char *file, char *args, FILE *fd)
{
    struct stat finfo;

    if(stat(file,&finfo) == -1) {
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
    evaluate_access(file,&finfo,M_GET,&allow,&allow_options, fd);
    if(!allow) {
        log_reason("client denied by server configuration",file);
        unmunge_name(file);
        die(FORBIDDEN,file,fd);
    }

    if(S_ISDIR(finfo.st_mode)) {
        char ifile[MAX_STRING_LEN];

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
            if(file[strlen(file) - 1] != '/') {
                char url[MAX_STRING_LEN];
                strcpy_dir(ifile,file);
                unmunge_name(ifile);
                construct_url(url,ifile);
                die(REDIRECT,url,fd);
            }
            send_file(ifile,fd,args);
        }
        return;
    }
    if(S_ISREG(finfo.st_mode))
        send_file(file,fd,args);
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
        send_node(url,args,out);
        return;
      case REDIRECT_URL:
        die(REDIRECT,url,out);
      case SCRIPT_NCSA:
        exec_get_NCSA(url,args,out);
        return;
      case SCRIPT_CGI:
        exec_cgi_script(m,url,args,in,out);
        return;
    }
}
