/*
 * http_request.c: functions to get and process requests
 * 
 * All code contained herein is covered by the Copyright as distributed
 * in the README file in the main directory of the distribution of 
 * NCSA HTTPD.
 *
 * 03-21-95  Based on NCSA HTTPd 1.3 by Rob McCool
 * 
 * 04-19-95 blong
 *	Forgot to remove free(remote_ip) from here, like elsewhere
 *
 * 04-20-95 blong
 *	Added patch "B18" for redirects w/o delay by Robert Hartill
 *
 * 05-01-95 blong
 *	Added patch by Steve Abatangle (sabat@enterprise.DTS.Harris.COM)
 *	to log SIGPIPE and timed out differently in send_fd_timed_out
 */


#include "httpd.h"
#include <setjmp.h>
#include "new.h"

int assbackwards;
char *remote_host = NULL;
char *remote_ip = NULL;
char *remote_name = NULL;
FILE *time_out_fd;
extern JMP_BUF jmpbuffer;
extern int getline_seen_cmd;
extern int getline_buffered_fd;

/* If RFC931 identity check is on, the remote user name */
char *remote_logname;
static void (*exit_callback)();

void send_fd_timed_out(int sigcode) {
    char errstr[MAX_STRING_LEN];

    fclose(time_out_fd);
    if(exit_callback) (*exit_callback)();
    if (sigcode != SIGPIPE) {
      sprintf(errstr,"httpd: send timed out for %s",remote_name);
    } else {
      sprintf(errstr,"httpd: send aborted for %s",remote_name);
    }
    log_error(errstr);
    log_transaction();
    if (in_headers_env) {
	free_env(in_headers_env);
	in_headers_env = NULL;
    }
    if (!standalone) {
   	fclose(stdin); 
    	fclose(stdout); 
	exit(0);
    } else {
      if (remote_host) {
	free(remote_host);
	remote_host = NULL;
      }	
/* Duh.  Free memory in bss */
/*      if (remote_ip) free(remote_ip); */
      if (remote_name) {
	free(remote_name);
	remote_name = NULL;
      }
#if defined(NeXT) || defined(__mc68000__)
      longjmp(jmpbuffer,1);
#else
      siglongjmp(jmpbuffer,1);
#endif
    }
}

/*
  We'll make it return the number of bytes sent
  so that we know if we need to send a body by default
*/
long send_fd(FILE *f, FILE *fd, void (*onexit)())
{
    char buf[IOBUFSIZE];
    long total_bytes_sent;
    register int n,o,w;

    time_out_fd = f;
    exit_callback = onexit;
    signal(SIGALRM,send_fd_timed_out);
    signal(SIGPIPE,send_fd_timed_out); 

    total_bytes_sent = 0;
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
	    total_bytes_sent += w;
        }
    }
    fflush(fd);
    alarm(0);
    signal(SIGALRM,SIG_IGN);
    signal(SIGPIPE,SIG_IGN);
    return total_bytes_sent;
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

char method[HUGE_STRING_LEN];
char protocal[HUGE_STRING_LEN];
char the_request[HUGE_STRING_LEN];
char failed_request[HUGE_STRING_LEN];
char as_requested[HUGE_STRING_LEN];
char url2[HUGE_STRING_LEN];
char failed_url[HUGE_STRING_LEN];
char args2[HUGE_STRING_LEN];

void initialize_request() {

   /* reset security information to access config defaults */
    reset_security();
    reset_mime_vars();

   /* Initialize Error codes */ 
    ErrorStat = 0;
    status = 200;
    
    init_header_vars();
    reset_to_saved_aliases();
    exit_callback = NULL;
    as_requested[0] = '\0';
    failed_url[0] = '\0';
    failed_request[0] = '\0';
    local_default_type[0] = '\0';
    local_default_icon[0] = '\0';

/* All but HEAD send more than a header */
    header_only = 0;

    bytes_sent = -1;

/* Initialize buffered getline */
    getline_buffered_fd = -1;
    getline_seen_cmd = 0;

}


void process_request(int in, FILE *out) {
    get_remote_host(in);
    signal(SIGPIPE,send_fd_timed_out); 


    if(getline(as_requested,HUGE_STRING_LEN,in,timeout))
        return;
    if(!as_requested[0]) 
        return;

    strcpy(the_request, as_requested);

    getword(method,as_requested,' ');
    getword(args2,as_requested,' ');
    getword(url2,args2,'?');

    unescape_url(url2);
    getword(protocal,as_requested,'\0');

    if(protocal[0] != '\0') {
        assbackwards = 0;
        get_mime_headers(in,out, url2);
    }
    else
        assbackwards = 1;

    if(!strcmp(method,"HEAD")) {
        header_only=1;
        process_get(in,out,method,url2,args2);
    }
    else if(!strcmp(method,"GET")) {
        process_get(in,out,method,url2,args2);
    }
    else if(!strcmp(method,"POST")) {
        post_node(url2,args2,in,out);
    }
    else if(!strcmp(method,"PUT")) {
        put_node(url2,args2,in,out);
    }
    else if(!strcmp(method,"DELETE")) {
        delete_node(url2,args2,in,out);
    }
    else 
        die(BAD_REQUEST,"Invalid or unsupported method.",out);

}

