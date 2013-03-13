/*
 * http_log.c: Dealing with the logs and errors
 * 
 * Rob McCool
 * 
 */


#include "httpd.h"

static FILE *error_log;
static FILE *xfer_log;

void open_logs() {
    if(!(error_log = fopen(error_fname,"a"))) {
        fprintf(stderr,"httpd: could not open error log file %s.\n",
                error_fname);
        perror("fopen");
        exit(1);
    }
    /* Make sure nasty scripts or includes send errors here */
    if(fileno(error_log) != STDERR_FILENO)
        dup2(fileno(error_log),STDERR_FILENO);
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
    fclose(stderr);
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

void log_transaction(char *cmd_line) {
    if(!do_rfc931)
        fprintf(xfer_log, "%s [%s] %s\n", remote_name, get_time(), cmd_line);
    else
        fprintf(xfer_log, "%s@%s [%s] %s\n", remote_logname, remote_name, 
                get_time(), cmd_line);
    fclose(xfer_log); /* we should be done with it... */
}

void log_error(char *err) {
    fprintf(error_log, "[%s] %s\n",get_time(),err);
    fclose(error_log);
}

void log_reason(char *reason, char *file) {
    char t[MAX_STRING_LEN];

    sprintf(t,"httpd: access to %s failed for %s, reason: %s",
            file,remote_name,reason);
    log_error(t);
}

void begin_http_header(FILE *fd, char *msg) {
    fprintf(fd,"%s %s%c",SERVER_PROTOCOL,msg,LF);
    dump_default_header(fd);
}

void error_head(FILE *fd, char *err) {
    if(!assbackwards) {
        begin_http_header(fd,err);
        fprintf(fd,"Content-type: text/html%c",LF);
        fprintf(fd,"%c",LF);
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
      case AUTH_REQUIRED:
        if(!assbackwards) {
            begin_http_header(fd,"401 Unauthorized");
            fprintf(fd,"WWW-Authenticate: %s%c%c",err_string,LF,LF);
        }
        if(header_only) break;
        title_html(fd,"Authorization Required");
        fprintf(fd,"Browser not authentication-capable or %c",LF);
        fprintf(fd,"authentication failed.%c",LF);
        break;
      case BAD_REQUEST:
        error_head(fd,"400 Bad Request");
        if(header_only) break;
        fprintf(fd,"Your client sent a query that this server could not%c",LF);
        fprintf(fd,"understand.<P>%c",LF);
        fprintf(fd,"Reason: %s<P>%c",err_string,LF);
        break;
      case FORBIDDEN:
        error_head(fd,"403 Forbidden");
        if(header_only) break;
        fprintf(fd,"Your client does not have permission to get URL %s ",
                err_string);
        fprintf(fd,"from this server.<P>%c",LF);
        break;
      case NOT_FOUND:
        error_head(fd,"404 Not Found");
        if(header_only) break;
        fprintf(fd,"The requested URL %s was not found on this server.<P>%c",
                err_string,LF);
        break;
      case SERVER_ERROR:
        error_head(fd,"500 Server Error");
        log_error(err_string);
        if(header_only) 
            break;
        fprintf(fd,"The server encountered an internal error or%c",LF);
        fprintf(fd,"misconfiguration and was unable to complete your%c",LF);
        fprintf(fd,"request.<P>%c",LF);
        fprintf(fd,"Please contact the server administrator,%c",LF);
        fprintf(fd," %s ",server_admin);
        fprintf(fd,"and inform them of the time the error occured, and%c",LF);
        fprintf(fd,"anything you might have done that may have caused%c",LF);
        fprintf(fd,"the error.<P>%c",LF);
        break;
      case INCLUDE_ERROR:
/*        error_head(fd,"500 Server Error"); */
        log_error(err_string);
        if(header_only) break;
        fprintf(fd,"[we're sorry, the following error has occurred: %s]%c",
                err_string,LF);
        fflush(fd);
        htexit(1,fd);
        break;
      case NOT_IMPLEMENTED:
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
    }
    if(!header_only)
        fprintf(fd,"</BODY>%c",LF);
    fflush(fd);
    htexit(1,fd);
}
