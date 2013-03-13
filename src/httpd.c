/************************************************************************
 * NCSA HTTPd Server
 * Software Development Group
 * National Center for Supercomputing Applications
 * University of Illinois at Urbana-Champaign
 * 605 E. Springfield, Champaign, IL 61820
 * httpd@ncsa.uiuc.edu
 *
 * Copyright  (C)  1995, Board of Trustees of the University of Illinois
 *
 ************************************************************************
 *
 * httpd.c,v 1.131 1996/04/05 18:55:09 blong Exp
 *
 ************************************************************************
 *
 * httpd.c: simple http daemon for answering WWW file requests
 *
 * 
 * 03-21-93  Rob McCool wrote original code (up to NCSA HTTPd 1.3)
 * 05-17-95  NCSA HTTPd 1.4.1
 * 05-01-95  NCSA HTTPd 1.4.2
 * 11-10-95  NCSA HTTPd 1.5.0
 * 11-14-95  NCSA HTTPd 1.5.0a
 * 03-21-96  NCSA HTTPd 1.5.0c
 * 
 */


#include "config.h"
#include "portability.h"

#include <stdio.h>
#ifndef NO_STDLIB_H 
# include <stdlib.h>
#endif /* NO_STDLIB_H */
#ifndef NO_MALLOC_H
# ifdef NEED_SYS_MALLOC_H
#  include <sys/malloc.h>
# else
#  include <malloc.h>
# endif /* NEED_SYS_MALLOC_H */
#endif /* NO_MALLOC_H */
#include <string.h>
#include <errno.h>
#include <setjmp.h>
#include <fcntl.h>
#include <pwd.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/signal.h>
#include <sys/socket.h>
#ifndef NO_SYS_WAIT_H 
# include <sys/wait.h> 
#endif /* NO_SYS_WAIT_H */
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#ifdef NEED_SELECT_H
# include <sys/select.h>
#endif /* NEED_SELECT_H */
#ifndef NO_SYS_RESOURCE_H
# include <sys/resource.h>
#endif /* NO_SYS_RESOURCE_H */
#include "constants.h"
#include "fdwrap.h"
#include "allocate.h"
#include "httpd.h"
#include "http_request.h"
#include "http_config.h"
#include "host_config.h"
#include "http_log.h"
#include "http_auth.h"
#include "http_access.h"
#include "http_dir.h"
#include "http_ipc.h"
#include "http_mime.h"
#include "http_send.h"
#include "util.h"

JMP_BUF restart_buffer;
int mainSocket;
pid_t pgrp;
int debug_mode = FALSE;

/* Current global information per child, will need to be made
 * non-global for threading
 */
#ifndef NOT_READY
int Child=0;
int Alone=0;
/* To keep from being clobbered with setjmp */
JMP_BUF jmpbuffer;
int csd = -1;
KeepAliveData keep_alive;  /* global keep alive info */
#endif /* NOT_READY */

ChildInfo *Children;
int num_children = 0;

#ifndef NO_PASS
char donemsg[]="DONE";
#endif /* NO_PASS */

#if defined(KRB4) || defined(KRB5)
#define HAVE_KERBEROS
#endif /* defined(KRB4) || defined(KRB5) */


void htexit(per_request *reqInfo, int status, int die_type) 
{
#ifdef NO_SIGLONGJMP
    if(standalone || keep_alive.bKeepAlive) longjmp(jmpbuffer,die_type);
#else
    if(standalone || keep_alive.bKeepAlive) siglongjmp(jmpbuffer,die_type);
#endif /* NO_SIGLONGJMP */
    else {
	rflush(reqInfo);
	exit(status);
    }
}


/*
 *  detach: This function disassociates httpd from its inherited
 *          process group and forms its own process group. Failure
 *          to do this would make httpd susceptible to signals sent
 *          to the entire process group. Also disassociates from
 *          control terminal.
 */

void detach(void) 
{
    int x;

    chdir("/");

    /* to ensure we're not a process group leader, fork */
    if((x = fork()) > 0)
        exit(0);
    else if(x == -1) {
        fprintf(stderr,"HTTPd: unable to fork new process\n");
        perror("fork");
        exit(1);
    }

#ifndef NO_SETSID
    if((pgrp=setsid()) == -1) {
        fprintf(stderr,"HTTPd: setsid failed\n");
        perror("setsid");
        exit(1);
    }
#else
    if((pgrp=setpgrp(getpid(),0)) == -1) {
        fprintf(stderr,"HTTPd: setpgrp failed\n");
        perror("setpgrp");
        exit(1);
    }
#endif /* NO_SETSID */
}

static int Restart = FALSE;
static int Exit = FALSE;

void sig_term(void) 
{
    log_error("HTTPd: caught SIGTERM, shutting down",
	      gConfiguration->error_log);
#ifndef NO_KILLPG
    killpg(pgrp,SIGKILL);
#else
    kill(-pgrp,SIGKILL);
#endif /* NO_KILLPG */
    shutdown(mainSocket,2);
    close(mainSocket);
    Exit = TRUE;
}

#ifdef BSD
void ign(void) 
{
#ifndef NeXT
    int status;
#else
    union wait status;
#endif /* NeXT */
    pid_t pid;

    while( (pid = wait3(&status, WNOHANG, NULL)) > 0);
}
#endif /* BSD */

void bus_error(void) 
{
    log_error("HTTPd: caught SIGBUS, dumping core",gConfiguration->error_log);
    close_all_logs();
    chdir(core_dir);
    abort();         
    exit(1);
}

void seg_fault(void) 
{
    log_error("HTTPd: caught SIGSEGV, dumping core",gConfiguration->error_log);
    close_all_logs();
    chdir(core_dir);
    abort();
/*    exit(1); */
}

void dump_debug(void) {
  per_request *tmp;

  log_error("HTTPd: caught USR1, dumping debugging information",
	     gConfiguration->error_log);

  fprintf(gConfiguration->error_log,"  ReqInfo: %d\tSockbuf: %d\tCGbuf: %d\n",
	 req_count,sockbuf_count,cgibuf_count);
  tmp = gCurrentRequest;
  while (tmp != NULL) {
    fprintf(gConfiguration->error_log,"  Status: %d \t\t Bytes: %ld\n",tmp->status,
		tmp->bytes_sent);
    fprintf(gConfiguration->error_log,"  URL: %s \t Filename: %s Args: %s\n",
		tmp->url, tmp->filename, tmp->args);
    fprintf(gConfiguration->error_log,"  RequestLine: %s\n",the_request);
    fprintf(gConfiguration->error_log,"  Request: %s %s?%s %s\n",
		methods[tmp->method],tmp->url,tmp->args,
		protocals[tmp->http_version]);
    fprintf(gConfiguration->error_log,"  Host: %s \t IP: %s\n",
		tmp->remote_host, tmp->remote_ip);
    fprintf(gConfiguration->error_log,"  Content-type: %s \t Content-Length: %d\n",
		tmp->outh_content_type, tmp->outh_content_length);
    fprintf(gConfiguration->error_log,"  Refererer: %s\n",
		tmp->inh_referer);
    fprintf(gConfiguration->error_log,"  User Agent: %s\n",
		tmp->inh_agent);	
    if (tmp->hostInfo->server_hostname)
      fprintf(gConfiguration->error_log,"  ServerName: %s\n",
		tmp->hostInfo->server_hostname);
    tmp = tmp->next;
  }
  log_error("HTTPd: finished dumping debugging information",
		gConfiguration->error_log);
}

#ifdef PURIFY
void dump2(void) 
{
  log_error("HTTPd: caught USR2, dumping debugging information",
	     gConfiguration->error_log);
  purify_new_leaks();   
}
#endif /* PURIFY */
    

void restart(void) 
{
  if (Child) {
    if (!Alone) {
#ifdef NO_SIGLONGJMP
      longjmp(jmpbuffer,1);
#else
      siglongjmp(jmpbuffer,1);
#endif /* NO_SIGLONGJMP */
    }
    else 
      exit (-1);
  } else {
    Restart = TRUE;
#ifdef NO_SIGLONGJMP
    longjmp(restart_buffer,1);
#else
    siglongjmp(restart_buffer,1);
#endif /* NO_SIGLONGJMP */
  }
}

void set_signals(void) 
{
    signal(SIGSEGV,(void (*)(int))seg_fault);
    signal(SIGBUS,(void (*)(int))bus_error);
    signal(SIGTERM,(void (*)(int))sig_term);
    signal(SIGHUP,(void (*)(int))restart);
    signal(SIGUSR1,(void (*)(int))dump_debug);
#ifdef PURIFY
    signal(SIGUSR2,(void (*)(int))dump2);
#endif /* PURIFY */

#ifdef BSD
    signal(SIGCHLD,(void (*)(int))ign);
#else
    signal(SIGCHLD,SIG_IGN);
#endif /* BSD */
}



/* Reset group privileges, after rereading the config files
 * (our uid may have changed, and if so, we want the new perms).
 *
 * Don't reset the uid yet --- we do that only in the child process,
 * so as not to lose any root privs.  But we can set the group stuff
 * now, once, and avoid the overhead of doing all this on each
 * connection.
 *
 * rst 1/23/95
 */
  
void set_group_privs(void)
{
    struct passwd *pwent;
    int tmp_stand;
    per_request fakeit;
  
    fakeit.out = stdout;
    fakeit.hostInfo = gConfiguration;

    /* Only change if root. Changed to geteuid() so that setuid scripts, etc
     * can start the server and change from root 
     */ 
    if (!geteuid()) {
	/* Change standalone so that on error, we die, instead of siglongjmp */
	tmp_stand = standalone;
	standalone = 0;
	
	if ((pwent = getpwuid(user_id)) == NULL)
	    die(&fakeit, SC_CONF_ERROR,
		"couldn't determine user name from uid");
    
	/* Reset `groups' attributes. */
    
	if (initgroups(pwent->pw_name, group_id) == -1)
	    die(&fakeit, SC_CONF_ERROR,"unable to setgroups");

	if (setgid(group_id) == -1)
	    die(&fakeit, SC_CONF_ERROR,"unable to change gid");
 
	standalone = tmp_stand;
    }
}

/* More idiot speed-hacking --- the first time conversion makes the C
 * library open the files containing the locale definition and time
 * zone.  If this hasn't happened in the parent process, it happens in
 * the children, once per connection --- and it does add up.
 *
 * rst 1/23/95
 */
  
void speed_hack_libs(void) 
{
   time_t dummy_time_t = time(NULL);
   struct tm *dummy_time = localtime (&dummy_time_t);
   struct tm *other_dummy_time = gmtime (&dummy_time_t);
   char buf[MAX_STRING_LEN];
   
   strftime (buf, MAX_STRING_LEN, "%d/%b/%Y:%H:%M:%S", dummy_time);
   strftime (buf, MAX_STRING_LEN, "%d/%b/%Y:%H:%M:%S", other_dummy_time);
}
 
/*
 * This function blocks on the socket when in keepalive mode. It
 * is called for all requests after the first one. It returns when
 * another request is ready, or the timeout period is up.
 */

int wait_keepalive(int csd, KeepAliveData *kad)
{
    fd_set listen_set;
    struct timeval ka_timeout;
    int    val;

    ka_timeout.tv_sec = kad->nTimeOut;
    ka_timeout.tv_usec = 0;
    while (1) {
	FD_ZERO(&listen_set);
	FD_SET(csd,&listen_set);

	if ((val = select(csd + 1,&listen_set,NULL,NULL,&ka_timeout)) == -1)
	    continue;   /* For now, assume signal occurred */
	else if (!val) {
	    kad->bKeepAlive = 0;
	    return 0;
	}
	else 
	    return 1;
    }
}

void CompleteRequest(per_request *reqInfo, int pipe)
{

/* Changed from shutdown(csd,2) to allow kernel to finish sending data 
 * required on OSF/1 2.0 (Achille Hui (eillihca@drizzle.stanford.edu)) */
/*    shutdown(csd,0); */
    shutdown(csd,2);
    close(csd);
#ifndef NO_PASS
    if (pipe >= 0) { 
	write(pipe,donemsg,sizeof(donemsg));
        if (reqInfo != NULL) reqInfo->RequestFlags = 0;
	free_request(reqInfo,NOT_LAST);
	CloseAll();
	freeAllStrings(STR_REQ);
	kill_indexing(FI_LOCAL);
/*        current_process_size("CompleteRequest/2");  */
#ifdef QUANTIFY
/*	quantify_save_data(); */
#endif /* QUANTIFY */
    } else
#endif /* NO_PASS */
#ifdef QUANTIFY
/*	quantify_save_data(); */
#endif /* QUANTIFY */
#ifdef PROFILE
	exit (2);
#else
	exit (0);
#endif /* PROFILE */
}

void child_alone(int csd, struct sockaddr_in *sa_server, 
		 CLIENT_SOCK_ADDR *sa_client)
{
  static per_request *reqInfo = NULL;

#ifndef THREADED
    close(mainSocket);
#endif /* THREADED */

#ifdef PROFILE
    moncontrol(1);
#endif /* PROFILE */
#ifdef QUANTIFY
/*    quantify_clear_data();  */
    quantify_start_recording_data();
#endif /* QUANTIFY */
   
    Child = Alone = 1;
    standalone = 0;
    keep_alive.nCurrRequests = 0;

    /* Only try to switch if we're running as root */
    if(!geteuid()) {
    if (setuid(user_id) == -1) {
            per_request fakeit;
            fakeit.out = stdout;
	    die(&fakeit, SC_CONF_ERROR,"unable to change uid");
        }
    }

#ifndef THREADED
    close(0);
    close(1);	
    dup2(csd,0);
    dup2(csd,1);
#endif /* THREADED */
    
    remote_logname = (!do_rfc931 ? NULL :
		      rfc931((struct sockaddr_in *)sa_client,
			     sa_server));
    
#ifdef NO_SIGLONGJMP
    if (setjmp(jmpbuffer) != 0) {
#else
    if (sigsetjmp(jmpbuffer,1) != 0) {
#endif /* NO_SIGLONGJMP */
	/* wait_keepalive returns 0 if timeout */
	/*    CompleteRequest doesn't return */
	rflush(gCurrentRequest);
	kill_indexing(FI_LOCAL);
	if ((keep_alive.nMaxRequests 
	     && (++keep_alive.nCurrRequests >= keep_alive.nMaxRequests)) ||
	    !wait_keepalive(csd, &keep_alive)) {
	    CompleteRequest(gCurrentRequest,-1);
	}
    }

    while (1) {
	reqInfo = initialize_request(reqInfo);
        reqInfo->connection_socket = 0;
	reqInfo->in = 0;
        reqInfo->out = stdout;
	RequestMain(reqInfo);
	rflush(reqInfo);
	kill_indexing(FI_LOCAL);

	if (!keep_alive.bKeepAlive) {
	    CompleteRequest(reqInfo,-1);
	} else if ((keep_alive.nMaxRequests 
		  && (++keep_alive.nCurrRequests >= keep_alive.nMaxRequests)) 
		 || !wait_keepalive(csd, &keep_alive)) {
	    CompleteRequest(reqInfo,-1);
	}
    }
}

/* to keep from being clobbered by setjmp */
static int x;
static int val;  /* indicates if keep_alive should remain active */
#ifdef FD_LINUX
static int switch_uid = 0;
#endif /* FD_LINUX */

void child_main(int parent_pipe, SERVER_SOCK_ADDR *sa_server)
{
    static per_request *reqInfo = NULL;
    close(mainSocket);
   
#ifdef PROFILE
    moncontrol(1);
#endif /* PROFILE */

#ifdef QUANTIFY
/*    quantify_clear_data(); */
	quantify_start_recording_data();
#endif /* QUANTIFY */

    /* Only try to switch if we're running as root */
    if(!geteuid()) {
	/* set setstandalone to 0 so we die on error, and not sigjmp */
	standalone = 0;

#ifdef FD_LINUX
       /*
        * This is very tricky, because we want to switch real
        * and effective UID while retaining a saved uid.
        */

        /* First, make us set-uid. */
        if (setreuid(user_id, -1) == -1) {
            per_request fakeit;
            fakeit.out = stdout;
            die(&fakeit, SC_CONF_ERROR,"unable to change ruid");
        }
        /* Saved uid is now 0. Reset effective uid. */
        if (seteuid(user_id) == -1) {
            per_request fakeit;
            fakeit.out = stdout;
            die(&fakeit, SC_CONF_ERROR,"unable to change euid");
        }
        switch_uid = 1;
#else 
	if (setuid(user_id) == -1) {
	    per_request fakeit;
	    fakeit.out = stdout;
	    die(&fakeit, SC_CONF_ERROR,"unable to change uid");
	}
#endif /* FD_LINUX */
	standalone = 1;
    } 

#ifndef THREADED
    for(x=0;x<num_children;x++) {
	if (parent_pipe != Children[x].parentfd) close(Children[x].parentfd);
	if (parent_pipe != Children[x].childfd) close(Children[x].childfd);
    }
    
    free(Children);
#endif /* THREADED */

#ifdef NO_SIGLONGJMP
    if ((val = setjmp(jmpbuffer)) != 0) {
#else
    if ((val = sigsetjmp(jmpbuffer,1)) != 0) {
#endif /* NO_SIGLONGJMP */
        reqInfo = gCurrentRequest;
	rflush(reqInfo);
	kill_indexing(FI_LOCAL);
	if (val == DIE_KEEPALIVE) {
	    /* returns 0 if timeout during multiple request session */
	    if ((keep_alive.nMaxRequests 
		 && (++keep_alive.nCurrRequests >= keep_alive.nMaxRequests)) 
		|| !wait_keepalive(csd, &keep_alive)) {
		keep_alive.bKeepAlive = 0;    
		CompleteRequest(reqInfo,parent_pipe);
	    }
	}
	else { /* in case it was in effect. probably a better place to reset */
	    keep_alive.bKeepAlive = 0;    
	    CompleteRequest(reqInfo,parent_pipe);
	}
    }

    while (1) {
	alarm (0);
	if (!keep_alive.bKeepAlive) {
	    GetDescriptor (parent_pipe);
	    remote_logname = GetRemoteLogName(sa_server);
	    keep_alive.nCurrRequests = 0;
	    if (reqInfo != NULL) reqInfo->RequestFlags = 0;
	}

	reqInfo = initialize_request(reqInfo);
	reqInfo->connection_socket = 0;
	reqInfo->in = 0;
	reqInfo->out = stdout;
	RequestMain(reqInfo);
	rflush(reqInfo);
	kill_indexing(FI_LOCAL);

	if (!keep_alive.bKeepAlive) {
	    CompleteRequest(reqInfo,parent_pipe);
	} else if ((keep_alive.nMaxRequests 
		  && (++keep_alive.nCurrRequests >= keep_alive.nMaxRequests)) 
		 || !wait_keepalive(csd, &keep_alive)) {
	    keep_alive.bKeepAlive = 0;
	    CompleteRequest(reqInfo,parent_pipe);
	}
    }    
}

#ifndef NO_PASS
void GetDescriptor(int parent_pipe)
{
#ifndef THREADED
    dup2(parent_pipe,0);
    dup2(parent_pipe,1);
#endif /* THREADED */
#ifdef SETPROCTITLE
    setproctitle("idle"); 
#endif /* SETPROCTITLE */

#ifdef FD_LINUX
    /* Switch to root privilige temporarily */
    if (switch_uid && seteuid(0) < 0) {
        log_error("child error: seteuid(0)",gConfiguration->error_log);
        close(0);
        close(1);
        close(csd);
        close(parent_pipe);
        exit(1); 
       }
#endif /* FD_LINUX */

    csd = recv_fd(parent_pipe);

#ifdef FD_LINUX
    /* Give up priviliges. */
    if (switch_uid && seteuid(user_id) < 0) {
	log_error("unable to change uid", gConfiguration->error_log);
        csd = -1;
    }
#endif /* FD_LINUX */
    if (csd < 0) {
	log_error("child error: recv_fd()",gConfiguration->error_log);
	close(0);
	close(1);
	close(mainSocket);
	close(parent_pipe);
	exit(1);
    }
#ifndef THREADED
    close(0);
    close(1);	
    dup2(csd,0);
    dup2(csd,1);
#endif /* THREADED */

}
#endif /* NO_PASS */

char* GetRemoteLogName (SERVER_SOCK_ADDR *sa_server)
{
    if (do_rfc931) {
      CLIENT_SOCK_ADDR sa_client;
      int addrlen;
	
      addrlen = sizeof sa_client;
      getpeername(csd,(struct sockaddr *) &sa_client, &addrlen);
      return rfc931((struct sockaddr_in *) &sa_client,
		    sa_server);
    } 
    else
      return NULL;
}

#ifndef NO_PASS
int make_child(int argc, char **argv, int childnum,
		SERVER_SOCK_ADDR *sa_server)
{
    int fd[2];
    int pid;
#ifdef SETPROCTITLE
    char namestr[30];
#endif /* SETPROCTITLE */

    pid = 1;
#ifndef NEED_SPIPE
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == -1) {
#else
    if (s_pipe(fd)  == -1) {
#endif /* NEED_SPIPE */
	log_error("Unable to open socket for new process",
		  gConfiguration->error_log);
	return -1;
    } else {
	if ((pid = fork()) == -1) {
	    log_error("Unable to fork new process",gConfiguration->error_log);
	    close(fd[1]);
	    close(fd[0]);
	} else {
	    Children[childnum].childfd = fd[1];
	    Children[childnum].parentfd = fd[0];
	    Children[childnum].pid = pid;
	    Children[childnum].busy = 0;
	}
    
	if (!pid) {
	 /* Child */
	    close(Children[childnum].childfd); 
#ifdef BSD
	    signal(SIGCHLD,(void (*)())ign);
#else
	    signal(SIGCHLD,SIG_IGN);
#endif /* BSD */
#ifdef SETPROCTITLE
	    sprintf(namestr,"child %d",childnum);
 	    setproctitle(namestr); 
#endif /* SETPROCTITLE */
	    Child = 1;
	    child_main(Children[childnum].parentfd, sa_server);
	} else {
	  /* Parent */
	    close(Children[childnum].parentfd);
	}
    }
    return childnum;
}
#endif /* NO_PASS */


void initialize_socket(SERVER_SOCK_ADDR *sa_server, 
		       CLIENT_SOCK_ADDR *sa_client)
{
  int one=1;    
  int keepalive_value = 1;  

  if ((mainSocket = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP)) == -1) {
    fprintf(stderr,"HTTPd: could not get socket\n");
    perror("socket");
    exit(1);
  }

  if((setsockopt(mainSocket,SOL_SOCKET,SO_REUSEADDR,(char *)&one,
		 sizeof(one))) == -1) {
    fprintf(stderr,"HTTPd: could not set socket option SO_REUSEADDR\n");
    perror("setsockopt");
    exit(1);
  }

  /* Added at the suggestion of SGI, seems some PC clients don't close the 
     connection, which causes IRIX and Solaris to keep sending them packets
     for a really long time . . . 
     Sent in From: Brian Behlendorf <brian@wired.com> */

  if((setsockopt(mainSocket,SOL_SOCKET,SO_KEEPALIVE,(void *)&keepalive_value,
		 sizeof(keepalive_value))) == -1) {
    fprintf(stderr,"HTTPd: could not set socket option SO_KEEPALIVE\n"); 
    perror("setsockopt"); 
    exit(1); 
  }
  
  bzero((char *) sa_server, sizeof(*sa_server));
  sa_server->sin_family=AF_INET;
  sa_server->sin_addr= gConfiguration->address_info; 
  /*    sa_server.sin_addr.s_addr=htonl(INADDR_ANY); */
  sa_server->sin_port=htons(port);
  if(bind(mainSocket,(struct sockaddr *) sa_server,sizeof(*sa_server)) == -1) {
    if (gConfiguration->address_info.s_addr != htonl(INADDR_ANY)) 
      fprintf(stderr,"HTTPd: cound not bind to address %s port %d\n",
	      inet_ntoa(gConfiguration->address_info),port);
    else
      fprintf(stderr,"HTTPd: could not bind to port %d\n",port);
    perror("bind");
    exit(1);
  }
  listen(mainSocket,35);
}

void standalone_main(int argc, char **argv) 
{
    static char msg[10];
    static fd_set listen_set;
    static int csd, clen;
    static int x, num_sigs, nread;
    static int max, free_child;
    static int last_child = 0;
    static int no_child_busy = 0;
    static int search_cnt;
    static SERVER_SOCK_ADDR sa_server;
    static CLIENT_SOCK_ADDR sa_client;
    static int    one = 1;
    static int pid = 0;
#ifdef TACHOMETER
    static int Requests[MAX_TACHOMETER];
    static time_t Request_time = 0L;
    static int i;
    static char Request_title[64];
#endif /* TACHOMETER */



   /* Some systems need to call TZSET before detaching from the shell
      process to ensure proper time zone settings */
    tzset();

    if (debug_mode == FALSE) 
      detach();    

    sprintf(error_msg,"HTTPd: Starting as %s",argv[0]);
    x = 1;
    while (x < argc)
      sprintf(error_msg,"%s %s",error_msg, argv[x++]);
    log_error(error_msg, gConfiguration->error_log);


#ifdef SETPROCTITLE
    setproctitle("Accepting Connections"); 
# ifdef TACHOMETER
    for (i=0;i<MAX_TACHOMETER;i++)
      Requests[i] = 0;
    Request_time = time(NULL);
    Request_time -= Request_time%60;
# endif /* TACHOMETER */
#endif /* SETPROCTITLE */

    while(!Exit) {
/*      current_process_size("Starting"); */
      initialize_socket(&sa_server,&sa_client);
      set_signals();
      speed_hack_libs();
      log_pid();

#ifndef NO_PASS
      num_children = 0;
      if (debug_mode == FALSE) {
        Children = (ChildInfo *) malloc(sizeof(ChildInfo)*(max_servers+1));
        while (num_children < start_servers) {
	  make_child(argc, argv, num_children++, &sa_server);
	}
      }
#endif /* NO_PASS */

#ifdef NO_SIGLONGJMP
      setjmp(restart_buffer);
#else
      sigsetjmp(restart_buffer,1);
#endif /* NO_SIGLONGJMP */

      while(!(Restart || Exit)) {
	FD_ZERO(&listen_set);

#ifndef RESOURCE_LIMIT
	FD_SET(mainSocket,&listen_set);
	max = mainSocket;
#else
	if (no_child_busy < max_servers) {
	  FD_SET(mainSocket,&listen_set);
	  max = mainSocket;
	} else max = 0;
#endif /* RESOURCE_LIMIT */

#ifndef NO_PASS
	for(x=0 ; x < num_children ; x++) {
	  FD_SET(Children[x].childfd, &listen_set);
	  if (Children[x].childfd > max) max = Children[x].childfd;
	}
#endif /* NO_PASS */

	if ((num_sigs = select(max+1,&listen_set,NULL,NULL,NULL))== -1) {
	    if (errno != EINTR) {
		fprintf(stderr,"Select error %d\n",errno);
		perror("select");
	    }
	} else {
#ifndef NO_PASS
	  for(x = 0; x < num_children ; x++) {
	    if (FD_ISSET(Children[x].childfd, &listen_set)) {
	      if ((nread = read(Children[x].childfd, msg, 10)) < 0) {
		log_error("child error: read msg failure",
			  gConfiguration->error_log);
	      } else if (nread == 0) {
		log_error("child error: child connection closed",
			  gConfiguration->error_log);
		close(Children[x].childfd);
		kill(Children[x].pid,SIGKILL);
		make_child(argc, argv, x, &sa_server);
	      } else {
		if (!strcmp(msg,donemsg)) {
		  Children[x].busy = 0;
		  no_child_busy--;
		} /* if strcmp */
	      } /* if nread else */
	    } /* if FD_ISSET */
	  } /* for x to num_children */
#endif /* NO_PASS */
	  if (FD_ISSET(mainSocket, &listen_set)) {
	    clen=sizeof(sa_client);
	    if((csd=accept(mainSocket,(struct sockaddr *)&sa_client,&clen)) == -1) {
	      if (errno == EINTR) {
#ifdef BSD			  
		ign();
#endif /* BSD */
	      } else {
		sprintf(error_msg,
			"HTTPd: socket error: accept failed %d",
			errno);
		log_error(error_msg, gConfiguration->error_log);
	      }
	    } else { /* connection accepted */

/* Fix for Solaris as suggested by Sun.  Not sure if it does anything
 * on other platforms, so its not currently #ifdef'd
 */
#ifdef TCP_NODELAY
		setsockopt(csd, IPPROTO_TCP, TCP_NODELAY, (void *)
			   &one, sizeof(one));
#endif /* TCP_NODELAY */
#ifndef NO_PASS
	      if (num_children) {
		/*free_child = 0;*/
		search_cnt = 0;
		free_child = last_child;
		if (num_children) {
		  while (Children[free_child].busy) {
		    free_child = (free_child + 1) % num_children;
		    search_cnt++;
		    /* free_child++ */
		    if (free_child == last_child)
		      break;
		  }
		}
		if (search_cnt >= num_children) {
		  if ( num_children < max_servers) {
		    if (make_child(argc, argv, num_children, &sa_server) >= 0) {
		      free_child=num_children;
		      Children[free_child].busy = 1;
		      no_child_busy++;
		      if (pass_fd(Children[free_child].childfd,csd) < 0)
			log_error("child error: pass failed",
				  gConfiguration->error_log);
		      last_child = 0;
		      num_children++;
		    } else {
		      log_error("Main error: make_child failed",
				gConfiguration->error_log);
		    } /* if (make_child) else ... */
		  } else { 
		    /* Already have as many children as compiled for.*/
		    if (debug_mode == FALSE) 
		      pid = fork();
		     else 
		      Exit = TRUE;
		    if (!pid) {
		      child_alone(csd,&sa_server,&sa_client);
		    } 
		  } /* if (num_children < max_servers) ... else ... */
		} else {
		  Children[free_child].busy = 1;
		  no_child_busy++;
		  last_child = (free_child + 1) % num_children;
		  if (pass_fd(Children[free_child].childfd,csd) < 0)
		    log_error("child error: pass failed",
			      gConfiguration->error_log);
		} /* if (search_cnt >= num_children) ... else ... */
		close(csd);	    
	      } else 
#endif /* NO_PASS */
		{
		  if (debug_mode == FALSE) 
		    pid = fork();
		   else
		    Exit = TRUE;
		  if (!pid) {
		    child_alone(csd,&sa_server,&sa_client);
		  } /* fork */
		  close(csd);
		} /* if (num_children) ... else ... */
#ifdef TACHOMETER
              Requests[0]++;
              if (time(NULL) - Request_time > 60) {
                int avg1 = Requests[0];
                int avg5 = 0;
                int avg30 = 0;
                Request_time += 60;
                for (i=MAX_TACHOMETER-1; i>0; i--) {
                  if ( i < 5 )
                    avg5 += Requests[i];
                  avg30 += Requests[i];
                  Requests[i] = Requests[i-1]; 
                }
                avg5 += Requests[0];
                avg30 += Requests[0];
                Requests[0] = 0;

                sprintf(Request_title, "Accepting Connections: %d/%d/%d",
                        avg1, avg5/5, avg30/30);
                setproctitle(Request_title); 
              }
#endif /* TACHOMETER */
	    } /* If good accept */
	  } /* if mainSocket ready for read */
	} /* if select */
      } /* while (!(Restart || Exit)) */
      if (Restart) {
	FILE *error_log;
	/* open up the old log file to dump errors from reading of logfiles,
	   then close.  Need to do this before free_host_conf which will 
	   destroy the old name. */
	shutdown(mainSocket,2);
	close(mainSocket);
        error_log = fopen(gConfiguration->error_fname,"a");
        log_error("HTTPd: caught SIGHUP, restarting",error_log);
        kill_mime();
        kill_security();
	kill_indexing(FI_GLOBAL);
#ifndef NO_PASS
        for(x=0;x<num_children;x++) {
	  close(Children[x].parentfd);
	  close(Children[x].childfd);
	  kill(Children[x].pid,SIGKILL);
        }
        free(Children);
#endif /* NO_PASS */        
        free_host_conf();
	freeAllStrings(STR_HUP);
	read_config(error_log);
	set_group_privs();
	log_error("HTTPd: successful restart",error_log);
	get_local_host();
	fclose(error_log);
	Restart = FALSE;
      } /* if (Restart) */
    } /* while (!Exit) */
} /* standalone_main */


void default_banner(FILE* fout)
{
  fprintf(fout,"NCSA HTTPd %s\n",SERVER_SOURCE);
  fprintf(fout,"Licensed material.  Portions of this work are\n");
  fprintf(fout,"Copyright (C) 1995-1996 Board of Trustees of the University of Illinois\n");
  fprintf(fout,"Copyright (C) 1995-1996 The Apache Group\n");
#ifdef DIGEST_AUTH
  fprintf(fout,"Copyright (C) 1989-1993 RSA Data Security, Inc.\n");
#endif /* DIGEST_AUTH */
#ifdef DIGEST_AUTH
  fprintf(fout,"Copyright (C) 1993-1994 Carnegie Mellon University\n");
  fprintf(fout,"Copyright (C) 1991      Bell Communications Research, Inc. (Bellcore)\n");
  fprintf(fout,"Copyright (C) 1994      Spyglass, Inc.\n");
#endif /* DIGEST_AUTH */
  fflush(fout);
}

void usage(char *bin) 
{
  default_banner(stderr);
  fprintf(stderr,"\nDocumentation online at http://hoohoo.ncsa.uiuc.edu/\n\n");
  
  fprintf(stderr,"Compiled in Options:\n");
#ifdef SETPROCTITLE
  fprintf(stderr,"\tSETPROCTITLE\n");
#ifdef TACHOMETER
  fprintf(stderr,"\tTACHOMETER\n");
#endif /* TACHOMETER */
#endif /* SETPROCTITLE */
#ifdef XBITHACK
  fprintf(stderr,"\tXBITHACK\n");
#endif /* XBITHACK */
#ifdef SECURE_LOGS
  fprintf(stderr,"\tSECURE_LOGS\n");
#endif /* SECURE_LOGS */
#ifdef NO_PASS
  fprintf(stderr,"\tNO_PASS\n");
#endif /* NO_PASS */
#ifdef DBM_SUPPORT
  fprintf(stderr,"\tDBM_SUPPORT\n");
#endif /* DBM_SUPPORT */
#ifdef DIGEST_AUTH
  fprintf(stderr,"\tDIGEST_AUTH\n");
#endif /* DIGEST_AUTH */
#ifdef CONTENT_MD5
  fprintf(stderr,"\tCONTENT_MD5\n");
#endif /* CONTENT_MD5 */
#ifdef KRB4
  fprintf(stderr,"\tKRB4\n");
#endif /* KRB4 */
#ifdef KRB5
  fprintf(stderr,"\tKRB5\n");
#endif /* KRB5 */
  fprintf(stderr,"\tHTTPD_ROOT = %s\n",HTTPD_ROOT);
  fprintf(stderr,"\tDOCUMENT_ROOT = %s\n", DOCUMENT_LOCATION);

  fprintf(stderr,"\n");
#ifdef HAVE_KERBEROS
  fprintf(stderr,"Usage: %s [-d directory] [-f file] [-k file] [-K file] [-vX]\n",bin);
#else
  fprintf(stderr,"Usage: %s [-d directory] [-f file] [-vX]\n",bin);
#endif /* HAVE_KERBEROS */
  fprintf(stderr,"-d directory\t : specify an alternate initial ServerRoot\n");
  fprintf(stderr,"-f file\t\t : specify an alternate ServerConfigFile\n");
  fprintf(stderr,"-X\t\t : Answer one request for debugging, don't fork\n");
  fprintf(stderr,"-v\t\t : version information (this screen)\n");
#ifdef KRB4
  fprintf(stderr,"-k file\t\t : specify an alternate Kerberos V4 svrtab file\n");
#endif /* KRB4 */
#ifdef KRB5
  fprintf(stderr,"-K file\t\t : specify an alternate Kerberos V5 svrtab file\n");
#endif /* KRB5 */
    exit(1);
}

extern char *optarg;
extern int optind;

int main (int argc, char **argv, char **envp)
{
    int c;

/* FIRST */
#ifdef RLIMIT_NOFILE
    /* If defined (not user defined, but in sys/resource.h), this will attempt
     * to set the max file descriptors per process as high as allowed under
     * the OS.  This is due to the large number of file descriptors HTTPd 
     * can use in several configurations.
     */
    struct rlimit rlp;
    
    getrlimit(RLIMIT_NOFILE, &rlp);
    rlp.rlim_cur = rlp.rlim_max;
    setrlimit(RLIMIT_NOFILE, &rlp);
#endif /* RLIMIT_NOFILE */

#ifdef PROFILE
    /* When we're running profiled, we do not want to collect
     * statistics on server startup --- each child process inherits
     * those counts, which means that startup overhead is massively
     * overrepresented in the final output.  This turns off collection
     * of profiling statistics; we turn it back on immediately after
     * the fork() in standalone_main().  (This means it *never* gets
     * turned on for people running under inetd, but they can't be
     * much concerned with performance anyway).
     */
    moncontrol(0);
#endif /* PROFILE */

    /* First things first */
    strcpy(server_root,HTTPD_ROOT);
    make_full_path(server_root,SERVER_CONFIG_FILE,server_confname);

#ifdef HAVE_KERBEROS
    while((c = getopt(argc,argv,"d:f:vk:K:Xs")) != -1) {
#else
    while((c = getopt(argc,argv,"d:f:vXs")) != -1) {
#endif /* HAVE_KERBEROS */
        switch(c) {
	  case 'X':
	    debug_mode = TRUE;
	    printf("Debug On\n");
	    break;
          case 'd':
            strcpy(server_root,optarg);
	    make_full_path(server_root,SERVER_CONFIG_FILE,server_confname);
            break;
          case 'f':
	    if (optarg[0] == '/') {
              strcpy(server_confname,optarg);
            } else {
	      char *cwd = getcwd(NULL,255);
	      make_full_path(cwd, optarg, server_confname);
	      if (cwd) free(cwd);
	    }  
            break;
          case 'v':
	    usage(argv[0]);
            exit(1);
#ifdef HAVE_KERBEROS
	  case 'k':
#ifdef KRB4
	    strcpy(k4_srvtab, optarg); 
	    break;
#else
	    fprintf(stderr,"Kerberos V4 not supported (srvtab arg ignored)\n");
	    break;
#endif /* KRB4 */
	case 'K':
#ifdef KRB5
	    strcpy(k5_srvtab, optarg);
	    break;
#else
	    fprintf(stderr,"Kerberos V5 not supported (srvtab arg ignored)\n");
	    break;
#endif /* KRB5 */
#endif  /* HAVE_KERBEROS */
          case '?':
            usage(argv[0]);
        }
    }


/* Global Initialization:
 * Currently for File Descriptor Table and allocater
 */
    InitFdTable();
    initialize_allocate();

    read_config(stderr);
/* Passed arguments stage, dump baloney */
#ifndef SUPPRESS_BANNER
    if (standalone) default_banner(stdout);
#endif /* SUPPRESS_BANNER */
#ifdef SETPROCTITLE
    initproctitle(process_name, argc, argv, envp); 
#endif /* SETPROCTITLE */

    set_group_privs();
    get_local_host();

#ifdef __QNX__
    dup2(0,1);
    dup2(0,2);
#endif /* __QNX */
    
    if(standalone)
        standalone_main(argc,argv);
    else {
	per_request *reqInfo;
	reqInfo = initialize_request(NULL);
        user_id = getuid();
        group_id = getgid();
	
	reqInfo->connection_socket = 0;
	reqInfo->in = 0;
	reqInfo->out = stdout;
        port = get_portnum(reqInfo,fileno(reqInfo->out));

        if(do_rfc931)
            remote_logname = get_remote_logname(reqInfo->out);

        RequestMain(reqInfo);
	rflush(reqInfo);
    }

    close_all_logs();
    fclose(stdin);
    fclose(stdout);
    return 0;
}
