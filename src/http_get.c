/*
 * http_get.c: Handles things associated with GET
 *
 * All code contained herein is covered by the Copyright as distributed
 * in the README file in the main directory of the distribution of 
 * NCSA HTTPD.
 * 
 * Based on NCSA HTTPd 1.3 by Rob McCool
 * 
 * 04-08-95  blong
 *	Fixed security hole which allowed a trailing slash on CGI_MAGIC_TYPE
 *	cgi anywhere scripts to send back the script contents.  Now the
 *	trailing slash is added to the PATH_INFO, and the script is run.
 *	Oh yeah, and don't forget about directories.
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
    bytes_sent = 0;
    if(!assbackwards) {
        set_content_length(fi->st_size);
        if (set_last_modified(fi->st_mtime,fd)) {
	  fclose(f);
	  return;
	}
        send_http_header(fd);
    }
    num_includes=0;
    if(!header_only) 
        send_fd(f,fd,NULL);
    log_transaction();
    fclose(f);
}

/* Almost exactly equalivalent to exec_cgi_script, but this one
   gets all of the path info passed to it, instead of calling get_path_info */

void send_cgi(char *method, char *file, char *path_args, char *args, 
              struct stat *finfo, int in, FILE *fd) 
{
    int m = M_GET;
    int stub_returns;

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
    if(!(in_headers_env = add_common_vars(in_headers_env,fd)))
        die(NO_MEMORY,"send_cgi",fd);

    bytes_sent = 0;
    stub_returns = cgi_stub(method,file,path_args,args,in_headers_env,finfo,in,fd);
    free_env(in_headers_env);
    in_headers_env = NULL;

    switch (stub_returns) {
        case REDIRECT_URL:
                die(REDIRECT,location,fd);
                break;
        case REDIRECT_LOCAL:
                break;
        default:
                log_transaction();
                break;
    }
}


void send_node(char *file, char *args, int in, FILE *fd)
{
    struct stat finfo;
    char pa[MAX_STRING_LEN];
    int length = 0;
    register x = 0;

/* Remove all of the trailing slashes from the filename in order
   to fix security hole.  All trailing slashes are placed in the 
   path alias (pa) array */

    length = strlen(file);
    while (length && (file[length-1] == '/') && (x < MAX_STRING_LEN)) {
	pa[x] = '/';
	x++;
	file[length-1] = '\0';
	length--;
    }
    pa[x] = '\0';
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
                strncat(pa,&file[l],MAX_STRING_LEN - strlen(pa));
                file[l] = '\0';
                send_cgi("GET",file,pa,args,&finfo,in,fd);
                return;
            } else if(!strcmp(content_type,INCLUDES_MAGIC_TYPE)) {
                if(stat(t,&finfo) == -1)
                    continue;
                l=strlen(t);
                strncat(pa,&file[l],MAX_STRING_LEN - strlen(pa));
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
/*
    probe_content_type(file);
    if(S_ISREG(finfo.st_mode) && (!strcmp(content_type,CGI_MAGIC_TYPE))) {
        send_cgi("GET",file,pa,args,&finfo,in,fd);
        return;
    }
*/

  send_regular: /* aaaaack */
    evaluate_access(file,&finfo,M_GET,&allow,&allow_options, fd);
    if(!allow) {
        log_reason("client denied by server configuration",file);
        unmunge_name(file);
        die(FORBIDDEN,file,fd);
    }

    if(S_ISDIR(finfo.st_mode)) {
        char ifile[HUGE_STRING_LEN];

/* Path Alias (pa) array should now have the trailing slash */
/*        if(file[strlen(file) - 1] != '/') {  */
        if (pa[0] != '/') {
            char url[HUGE_STRING_LEN];
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
    if(S_ISREG(finfo.st_mode)) {
      probe_content_type(file);
      if (!strcmp(content_type, CGI_MAGIC_TYPE))
	send_cgi("GET",file,pa,args,&finfo,in,fd);
       else 
        send_file(file,fd,&finfo,pa,args);
    } else {
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
