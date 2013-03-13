/*
 * http_log.c: Dealing with the logs and errors
 * 
 * Rob McCool
 * 
 */


#include "httpd.h"

FILE *error_log;
static FILE *xfer_log;

void open_logs() {
    if(!(error_log = fopen(error_fname,"a"))) {
        fprintf(stderr,"httpd: could not open error log file %s.\n",
                error_fname);
        perror("fopen");
        exit(1);
    }
    if(!(xfer_log = fopen(xfer_fname,"a"))) {
        fprintf(stderr,"httpd: could not open transfer log file %s.\n",
                xfer_fname);
        perror("fopen");
        exit(1);
    }
}

void close_logs() {
    fclose(xfer_log);
    fclose(error_log);
}

void error_log2stderr() {
    if(fileno(error_log) != STDERR_FILENO)
        dup2(fileno(error_log),STDERR_FILENO);
}

void log_pid() {
    FILE *pid_file;

    if(!(pid_file = fopen(pid_fname,"w"))) {
        fprintf(stderr,"httpd: could not log pid to file %s\n",pid_fname);
        exit(1);
    }
    fprintf(pid_file,"%d\n",getpid());
    fclose(pid_file);
}

static char the_request[HUGE_STRING_LEN];
int status;
int bytes_sent;

void record_request(char *cmd_line) {
    status = -1;
    bytes_sent = -1;

    strcpy(the_request,cmd_line);
}

void log_transaction() {
    char str[HUGE_STRING_LEN];
    long timz;
    struct tm *t;
    char tstr[MAX_STRING_LEN],sign;

    t = get_gmtoff(&timz);
    sign = (timz < 0 ? '-' : '+');
    if(timz < 0) 
        timz = -timz;

    strftime(tstr,MAX_STRING_LEN,"%d/%b/%Y:%H:%M:%S",t);

    sprintf(str,"%s %s %s [%s %c%02d%02d] \"%s\" ",
            remote_name,
            (do_rfc931 ? remote_logname : "-"),
            (user[0] ? user : "-"),
            tstr,
            sign,
            timz/3600,
            timz%3600,
            the_request);
    if(status != -1)
        sprintf(str,"%s%d ",str,status);
    else
        strcat(str,"- ");

    if(bytes_sent != -1)
        sprintf(str,"%s%d",str,bytes_sent);
    else
        strcat(str,"- ");
    fprintf(xfer_log,"%s\n",str);
    fclose(xfer_log);
}

void log_error(char *err) {
    fprintf(error_log, "[%s] %s\n",get_time(),err);
    fclose(error_log);
}

void log_error_noclose(char *err) {
    fprintf(error_log, "[%s] %s\n",get_time(),err);
    fflush(error_log);
}

void log_reason(char *reason, char *file) {
    char *buffer;

    buffer = (char *)malloc(strlen(reason)+strlen(remote_name)+
			    strlen(file)+50); 
    sprintf(buffer,"httpd: access to %s failed for %s, reason: %s",
            file,remote_name,reason);
    log_error(buffer);
    free(buffer);
}

void begin_http_header(FILE *fd, char *msg) {
    fprintf(fd,"%s %s%c",SERVER_PROTOCOL,msg,LF);
    dump_default_header(fd);
}

void error_head(FILE *fd, char *err) {
    if(!assbackwards) {
        begin_http_header(fd,err);
        fprintf(fd,"Content-type: text/html%c%c",LF,LF);
    }
    if(!header_only) {
        fprintf(fd,"<HEAD><TITLE>%s</TITLE></HEAD>%c",err,LF);
        fprintf(fd,"<BODY><H1>%s</H1>%c",err,LF);
    }
}

void title_html(FILE *fd, char *msg) {
    fprintf(fd,"<HEAD><TITLE>%s</TITLE></HEAD>%c",msg,LF);
    fprintf(fd,"<BODY><H1>%s</H1>%c",msg,LF);
}

void die(int type, char *err_string, FILE *fd) {
    char t[MAX_STRING_LEN];

    switch(type) {
      case REDIRECT:
        status = 302;
        if(!assbackwards) {
            begin_http_header(fd,"302 Found");
            fprintf(fd,"Location: %s%c",err_string,LF);
            fprintf(fd,"Content-type: text/html%c",LF);
            fputc(LF,fd);
        }
        if(header_only) break;
        title_html(fd,"Document moved");
        fprintf(fd,"This document has moved <A HREF=\"%s\">here</A>.<P>%c",
                err_string,LF);
        break;
      case USE_LOCAL_COPY:
        status = USE_LOCAL_COPY;
        begin_http_header(fd,"304 Not modified");
        fputc(LF,fd);
        header_only = 1;
        break;
      case AUTH_REQUIRED:
        status = 401;
        if(!assbackwards) {
            begin_http_header(fd,"401 Unauthorized");
            fprintf(fd,"Content-type: text/html%c",LF);
            fprintf(fd,"WWW-Authenticate: %s%c%c",err_string,LF,LF);
        }
        if(header_only) break;
        title_html(fd,"Authorization Required");
        fprintf(fd,"Browser not authentication-capable or %c",LF);
        fprintf(fd,"authentication failed.%c",LF);
        break;
      case BAD_REQUEST:
        status = 400;
        error_head(fd,"400 Bad Request");
        if(header_only) break;
        fprintf(fd,"Your client sent a query that this server could not%c",LF);
        fprintf(fd,"understand.<P>%c",LF);
        fprintf(fd,"Reason: %s<P>%c",err_string,LF);
        break;
      case FORBIDDEN:
        status = 403;
        error_head(fd,"403 Forbidden");
        if(header_only) break;
        fprintf(fd,"Your client does not have permission to get URL %s ",
                err_string);
        fprintf(fd,"from this server.<P>%c",LF);
        break;
      case NOT_FOUND:
        status = 404;
        error_head(fd,"404 Not Found");
        if(header_only) break;
        fprintf(fd,"The requested URL %s was not found on this server.<P>%c",
                err_string,LF);
        break;
      case SERVER_ERROR:
        status = 500;
        error_head(fd,"500 Server Error");
        log_error(err_string);
        if(header_only) 
            break;
        fprintf(fd,"The server encountered an internal error or%c",LF);
        fprintf(fd,"misconfiguration and was unable to complete your%c",LF);
        fprintf(fd,"request.<P>%c",LF);
        fprintf(fd,"Please contact the server administrator,%c",LF);
        fprintf(fd," %s ",server_admin);
        fprintf(fd,"and inform them of the time the error occurred, and%c",LF);
        fprintf(fd,"anything you might have done that may have caused%c",LF);
        fprintf(fd,"the error.<P>%c",LF);
        break;
      case NOT_IMPLEMENTED:
        status = 501;
        error_head(fd,"501 Not Implemented");
        if(header_only) break;
        fprintf(fd,"We are sorry to be unable to perform the method %s",
                err_string);
        fprintf(fd," at this time.<P>%c",LF);
        fprintf(fd,"If you would like to see this capability in future%c",LF);
        fprintf(fd,"releases, send the method which failed, why you%c",LF);
        fprintf(fd,"would like to have it, and the server version %s%c",
                SERVER_VERSION,LF);
        fprintf(fd,"to <ADDRESS>%s</ADDRESS><P>%c",SERVER_SUPPORT,LF);
        break;
      case NO_MEMORY:
        log_error("httpd: memory exhausted");
        status = 500;
        error_head(fd,"500 Server Error");
        if(header_only) break;
        fprintf(fd,"The server has temporarily run out of resources for%c",LF);
        fprintf(fd,"your request. Please try again at a later time.<P>%c",LF);
        break;
    }
    if(!header_only)
        fprintf(fd,"</BODY>%c",LF);
    fflush(fd);
    log_transaction();
    htexit(1,fd);
}
