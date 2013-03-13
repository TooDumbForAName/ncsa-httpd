/*
 * http_log.c: Dealing with the logs and errors
 * 
 * All code contained herein is covered by the Copyright as distributed
 * in the README file in the main directory of the distribution of 
 * NCSA HTTPD.
 *
 * Based on NCSA HTTPd 1.3 by Rob McCool
 * 
 * 10/28/94 cvarela
 *    Added agent_log and referer_log files.
 *
 * 02/15/95 blong
 *    Added support for configuration defined error messages
 * 
 * 03/19/95 blong
 *    Some modification to status lines for uniformity, and for user
 *    defined error messages to give the correct status.
 *
 * 04/11/95 blong
 *    Changed custom error responses to only send the error string as
 *    arguments to the script
 *
 * 05/08/95 blong
 *    Bowing to pressure, added patch by Paul Phillips (paulp@cerf.net)
 *    to set CLOSE_ON_EXEC flag for log files under #define SECURE_LOGS
 *
 * 06/01/95 blong
 *    Changed die() so that it only logs the transaction on non-ErrorDocument
 *    errors (errors that are handled internally to the server)
 */


#include "httpd.h"
#include "new.h"

const char StatLine200[] = "200 Document follows";
const char StatLine302[] = "302 Found";
const char StatLine304[] = "304 Not modified";
const char StatLine400[] = "400 Bad Request";
const char StatLine401[] = "401 Unauthorized";
const char StatLine403[] = "403 Forbidden";
const char StatLine404[] = "404 Not Found";
const char StatLine500[] = "500 Server Error";
const char StatLine501[] = "501 Not Implemented";

/* Moved to http_request.c */
/* static char the_request[HUGE_STRING_LEN]; */
extern char the_request[HUGE_STRING_LEN];
int status;
int bytes_sent;

FILE *error_log;
/* static FILE *xfer_log; */
static int xfer_log;
static int xfer_flags = ( O_WRONLY | O_APPEND | O_CREAT );
static mode_t xfer_mode = ( S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );
FILE *agent_log;
FILE *referer_log;
int ErrorStat=0;
ErrorMessage Errors[NUM_ERRORS];
int numErrorsDefined=0;
extern int servernum;
extern char referer[];
extern char *save_name;

void open_logs() {
    int flags;
    if(!(error_log = fopen(error_fname,"a"))) {
        fprintf(stderr,"httpd: could not open error log file %s.\n",
                error_fname);
        perror("fopen");
        exit(1);
    }
    if((xfer_log = open(xfer_fname,xfer_flags,xfer_mode)) < 0) {
        fprintf(stderr,"httpd: could not open transfer log file %s.\n",
                xfer_fname);
        perror("open");
        exit(1);
    }
    if(!(agent_log = fopen(agent_fname,"a"))) {
        fprintf(stderr,"httpd: could not open agent log file %s.\n",
                agent_fname);
        perror("fopen");
        exit(1);
    }
    if(!(referer_log = fopen(referer_fname,"a"))) {
        fprintf(stderr,"httpd: could not open referer log file %s.\n",
                referer_fname);
        perror("fopen");
        exit(1);
    }
   
#ifdef SECURE_LOGS
    /* set close-on-exec flag so CGI's cannot get to logs */
    flags = fcntl(xfer_log, F_GETFD);
    flags |= FD_CLOEXEC;
    fcntl(xfer_log, F_SETFD, flags);
    flags = fcntl(fileno(agent_log), F_GETFD);
    flags |= FD_CLOEXEC;
    fcntl(fileno(agent_log), F_SETFD, flags);
    flags = fcntl(fileno(referer_log), F_GETFD);
    flags |= FD_CLOEXEC;
    fcntl(fileno(referer_log), F_SETFD, flags);
#endif /* SECURE_LOGS */
}

void close_logs() {
    fflush(error_log);
    fflush(agent_log);
    fflush(referer_log);
    close(xfer_log);
    fclose(error_log);
    fclose(agent_log);
    fclose(referer_log);
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

/*
void record_request(char *cmd_line) {
    bytes_sent = -1;

    strcpy(the_request,cmd_line);
}
*/

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
        sprintf(str,"%s%d\n",str,bytes_sent);
    else
        strcat(str,"-\n");
/*    sprintf(str,"%s SN:%d",str,servernum);
    fprintf(xfer_log,"%s\n",str);
    fflush(xfer_log); */
    write(xfer_log,str,strlen(str));
}

void log_error(char *err) {
    fprintf(error_log, "[%s] %s\n",get_time(),err);
    fflush(error_log);
}

void log_error_noclose(char *err) {
    fprintf(error_log, "[%s] %s\n",get_time(),err);
    fflush(error_log);
}

void log_reason(char *reason, char *file) {
    char *buffer;

    buffer = (char *)malloc(strlen(reason)+strlen(referer)+strlen(remote_name)+
			    strlen(file)+50); 
    sprintf(buffer,"httpd: access to %s failed for %s, reason: %s from %s",
            file,remote_name,reason,( (referer[0] != '\0') ? referer : "-"));
    log_error(buffer);
    free(buffer);
}

void begin_http_header(FILE *fd, const char *msg) {
    fprintf(fd,"%s %s%c",SERVER_PROTOCOL,msg,LF);
    dump_default_header(fd);
}

void error_head(FILE *fd, const char *err) {
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

int die(int type, char *err_string, FILE *fd) {
    char error_doc[MAX_STRING_LEN];
    char arguments[HUGE_STRING_LEN];
    int RetVal=0;
    int x;

    /* For custom error scripts */
    strcpy(failed_request,the_request);
    strcpy(failed_url,url2);

    /* For 1.4b4, changed to have a common message for ErrorDocument calls
       We now send only error=err_string (as passed) and the CGI environment
       variable ERROR_STATUS,ERROR_REQUEST,ERROR_URL contain the rest of the
       relevent information */

    if (err_string)
      sprintf(arguments,"error=%s",err_string);

    switch(type) {
      case REDIRECT:
        status = 302;
	if (((x=have_error(type)) >= 0) && (!ErrorStat)) {
	    ErrorStat = status;
	    strcpy(error_doc,Errors[x].ErrorFile);
	    process_get(0,fd,"GET",error_doc,arguments);
	} else {
	    if(!assbackwards) {
		begin_http_header(fd,StatLine302);
		fprintf(fd,"Location: %s%c",err_string,LF);
		fprintf(fd,"Content-type: text/html%c",LF);
		fputc(LF,fd);
	    }
	    if (!header_only) {
	      title_html(fd,"Document moved");
	      fprintf(fd,"This document has moved <A HREF=\"%s\">here</A>.<P>%c",
		    err_string,LF);
              fprintf(fd,"</BODY>%c",LF);
	    }
	    log_transaction();
	}
        break;
      case USE_LOCAL_COPY:
        status = USE_LOCAL_COPY;
        begin_http_header(fd,StatLine304);
        fputc(LF,fd);
        header_only = 1;
	RetVal = USE_LOCAL_COPY;
	log_transaction();
        break;
      case AUTH_REQUIRED:
        status = 401;
	if (((x=have_error(type)) >= 0) && (!ErrorStat)) {
	    ErrorStat = status;
	    strcpy(error_doc,Errors[x].ErrorFile);
	    process_get(0,fd,"GET",error_doc,arguments);
	} else {
	    if(!assbackwards) {
		begin_http_header(fd,StatLine401);
		fprintf(fd,"Content-type: text/html%c",LF);
		fprintf(fd,"WWW-Authenticate: %s%c%c",err_string,LF,LF);
	    }
	    if (!header_only) {
	      title_html(fd,"Authorization Required");
	      fprintf(fd,"Browser not authentication-capable or %c",LF);
	      fprintf(fd,"authentication failed.%c",LF);
              fprintf(fd,"</BODY>%c",LF);
 	    }
	    log_transaction();
	}
        break;
      case BAD_REQUEST:
        status = 400;
	if (((x=have_error(type)) >= 0) && (!ErrorStat)) {
	    ErrorStat = status;
	    strcpy(error_doc,Errors[x].ErrorFile);
	    process_get(0,fd,"GET",error_doc,arguments);
	} else {
	    error_head(fd,StatLine400);
	    if (!header_only) {
	      fprintf(fd,"Your client sent a query that this server could not%c",LF);
	      fprintf(fd,"understand.<P>%c",LF);
	      fprintf(fd,"Reason: %s<P>%c",err_string,LF);
              fprintf(fd,"</BODY>%c",LF);
	    } 
	    log_transaction();
	}
	break;
      case FORBIDDEN:
        status = 403;
	if (((x=have_error(type)) >= 0) && (!ErrorStat)) {
	    ErrorStat = status;
	    strcpy(error_doc,Errors[x].ErrorFile);
	    process_get(0,fd,"GET",error_doc,arguments);
	} else {
	    error_head(fd,StatLine403);
	    if (!header_only) {
	      fprintf(fd,"Your client does not have permission to get URL %s ",
		    err_string);
	      fprintf(fd,"from this server.<P>%c",LF);
              fprintf(fd,"</BODY>%c",LF);
	    }	
	    log_transaction();
	}
	break;
      case NOT_FOUND:
        status = 404;
	if (((x=have_error(type)) >= 0) && (!ErrorStat)) {
	    ErrorStat = status;
	    strcpy(error_doc,Errors[x].ErrorFile);
	    process_get(0,fd,"GET",error_doc,arguments);
	} else {
	    error_head(fd,StatLine404);
	    if (!header_only) {
	      fprintf(fd,"The requested URL %s was not found on this server.<P>%c",
		    err_string,LF);
              fprintf(fd,"</BODY>%c",LF);
	    } 
	    log_transaction();
	}
        break;
      case SERVER_ERROR:
        status = 500;
	if (((x=have_error(type)) >= 0) && (!ErrorStat)) {
	    ErrorStat = status;
	    strcpy(error_doc,Errors[x].ErrorFile);
	    process_get(0,fd,"GET",error_doc,arguments);
	} else {
	    error_head(fd,StatLine500);
	    if (standalone)
		log_error_noclose(err_string);
	    else log_error(err_string);
	    if (!header_only) {
	      fprintf(fd,"The server encountered an internal error or%c",LF);
	      fprintf(fd,"misconfiguration and was unable to complete %c",LF);
	      fprintf(fd,"your request.<P>%c",LF);
	      fprintf(fd,"Please contact the server administrator,%c",LF);
	      fprintf(fd," %s ",server_admin);
	      fprintf(fd,"and inform them of the time the error occurred%c",LF);
	      fprintf(fd,", and anything you might have done that may%c",LF);
	      fprintf(fd,"have caused the error.<P>%c",LF);
	      fprintf(fd,"<b>Error:</b> %s%c",err_string,LF);
              fprintf(fd,"</BODY>%c",LF);
	    } 
	    log_transaction();
	}
        break;
      case NOT_IMPLEMENTED:
        status = 501;
	if (((x=have_error(type)) >= 0) && (!ErrorStat)) {
	    ErrorStat = status;
	    strcpy(error_doc,Errors[x].ErrorFile);
	    process_get(0,fd,"GET",error_doc,arguments);
	} else {
	    error_head(fd,StatLine501);
	    if (!header_only) {
	      fprintf(fd,"We are sorry to be unable to perform the method %s",
		    err_string);
	      fprintf(fd," at this time.<P>%c",LF);
	      fprintf(fd,"If you would like to see this capability in %c",LF);
	      fprintf(fd,"future releases, send the method which failed,%c",LF);
	      fprintf(fd,"why you would like to have it, and the server %c",LF);
	      fprintf(fd,"version %s to <ADDRESS>%s</ADDRESS><P>%c",
			SERVER_VERSION,SERVER_SUPPORT,LF);
              fprintf(fd,"</BODY>%c",LF);
	    }
	    log_transaction();
	}
        break;
      case NO_MEMORY:
        log_error("httpd: memory exhausted");
        status = 500;
	if (((x=have_error(type)) >= 0) && (!ErrorStat)) {
	    ErrorStat = status;
	    strcpy(error_doc,Errors[x].ErrorFile);
	    process_get(0,fd,"GET",error_doc,arguments);
	} else {
	    error_head(fd,StatLine500);
	    if (!header_only) {
	      fprintf(fd,"The server has temporarily run out of resources%c",LF);
	      fprintf(fd,"for your request. Please try again at a later time.<P>%c",LF);
              fprintf(fd,"</BODY>%c",LF);
	    }
            log_transaction();
	}
	break;
      case CONF_ERROR:
	sprintf(arguments,"httpd: configuration error = %s",err_string);
	log_error(arguments);
	error_head(fd,StatLine500);
	if (!header_only) {
	  fprintf(fd,"The server has encountered a misconfiguration.%c",LF);
	  fprintf(fd,"The error was %s.%c",err_string,LF);
          fprintf(fd,"</BODY>%c",LF);
        }
	log_transaction();
    }
    fflush(fd);
    if (!RetVal) 
      htexit(1,fd);
     else return RetVal;
}


/* Add error to error table.  Should be called from http_config.c at startup */
int add_error(char* errornum, char* name) {
    char *tmp;

    tmp = (char *)malloc(strlen(name)+1);
    strcpy(tmp,name);
    
/*    Errors[numErrorsDefined].Type = type; */
    Errors[numErrorsDefined].ErrorNum = atoi(errornum);
    Errors[numErrorsDefined].ErrorFile = tmp;

    return ++numErrorsDefined;
}

/* Do we have a defined error for errornum? */
int have_error(int errornum) {
    int x=0;

    while ((x < numErrorsDefined) && (Errors[x].ErrorNum != errornum)) {
	x++;}

    if (Errors[x].ErrorNum == errornum) return x;
    else return -1;
}

/* Reset Error Types (for SIGHUP-restart) */
void reset_error() {
    int x=0;

    for (x = 0 ; x < numErrorsDefined ; x++) {
      free(Errors[x].ErrorFile);
      Errors[x].ErrorNum = 0;
    }

    numErrorsDefined = 0;
}
