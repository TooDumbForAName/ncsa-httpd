/*
 * http_request.c: functions to get and process requests
 * 
 * Rob McCool 3/21/93
 * 
 */


#include "httpd.h"

int assbackwards;
char *remote_host;
char *remote_ip;
char *remote_name;

/* If RFC931 identity check is on, the remote user name */
char *remote_logname;
static void (*exit_callback)();

void send_fd_timed_out() {
    char errstr[MAX_STRING_LEN];

    if(exit_callback) (*exit_callback)();
    sprintf(errstr,"httpd: send timed out for %s",remote_name);
    log_error(errstr);
    log_transaction();
    fclose(stdin);
    fclose(stdout);
    exit(0);
}

void send_fd(FILE *f, FILE *fd, void (*onexit)())
{
    char buf[IOBUFSIZE];
    register int n,o,w;

    exit_callback = onexit;
    signal(SIGALRM,send_fd_timed_out);
    signal(SIGPIPE,send_fd_timed_out);

    while (1) {
        alarm(timeout);
        if((n=fread(buf,sizeof(char),IOBUFSIZE,f)) < 1) {
            break;
        }
        o=0;
        if(bytes_sent != -1)
            bytes_sent += n;
        while(n) {
            w=fwrite(&buf[o],sizeof(char),n,fd);
            n-=w;
            o+=w;
        }
    }
    fflush(fd);
}

int find_script(char *method, char *name, char *args, int in, FILE *out) 
{
    int n=count_dirs(name),i;
    char t[HUGE_STRING_LEN],ct_bak[MAX_STRING_LEN];
    struct stat finfo;
    
    strcpy(ct_bak,content_type);
    for(i=n;i;--i) {
        make_dirstr(name,i,t);
        probe_content_type(t);
        if(!strcmp(content_type,CGI_MAGIC_TYPE)) {
            char pa[HUGE_STRING_LEN];
            int l=strlen(t);
            
            if(stat(t,&finfo) == -1)
                continue;
            if(!(S_ISREG(finfo.st_mode)))
                return 0;
            strcpy(pa,&name[l]);
            name[l] = '\0';
            strcpy(content_type,ct_bak);
            send_cgi(method,name,pa,args,&finfo,in,out);
            return 1;
        }
    }
    return 0;
}


void process_request(int in, FILE *out) {
    char m[HUGE_STRING_LEN];
    char w[HUGE_STRING_LEN];
    char l[HUGE_STRING_LEN];
    char url[HUGE_STRING_LEN];
    char args[HUGE_STRING_LEN];
    int s,n;

    get_remote_host(in);
    exit_callback = NULL;
    signal(SIGPIPE,send_fd_timed_out);

#ifdef PEM_AUTH
    doing_pem = -1;
  handle_request:
#endif
    l[0] = '\0';
    if(getline(l,HUGE_STRING_LEN,in,timeout))
        return;
    if(!l[0]) 
        return;

    record_request(l);
    getword(m,l,' ');
    getword(args,l,' ');
    getword(url,args,'?');
/*    plustospace(url); */
    unescape_url(url);
    getword(w,l,'\0');
    init_header_vars();
    if(w[0] != '\0') {
        assbackwards = 0;
        get_mime_headers(in,out);
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
                s2 = decrypt_request(in,url,&out);
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
        post_node(url,args,in,out);
    }
    else if(!strcmp(m,"PUT")) {
        header_only = 0;
        put_node(url,args,in,out);
    }
    else if(!strcmp(m,"DELETE")) {
        header_only = 0;
        delete_node(url,args,in,out);
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
