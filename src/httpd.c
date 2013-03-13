/*
 * httpd.c: simple http daemon for answering WWW file requests
 *
 * All code contained herein is covered by the Copyright as distributed
 * in the README file in the main directory of the distribution of 
 * NCSA HTTPD.
 *
 * 
 * 03-21-93  Rob McCool wrote original code (up to NCSA HTTPd 1.3)
 * 
 * 03-06-95  blong
 *  changed server number for child-alone processes to 0 and changed name
 *   of processes
 *
 * 03-10-95  blong
 * 	Added numerous speed hacks proposed by Robert S. Thau (rst@ai.mit.edu) 
 *	including set group before fork, and call gettime before to fork
 * 	to set up libraries.
 *
 * 04-28-95  guillory
 *	Changed search pattern on child processes to better distribute load
 *
 * 04-30-95  blong
 *	added patch by Kevin Steves (stevesk@mayfield.hp.com) to fix
 *	rfc931 logging.  We were passing sa_client, but this information
 *	wasn't known yet at the time of the pass to the child.  Now uses
 *	getpeername in child_main to find this information.
 */


#include "httpd.h"
#include <sys/types.h>
#include <sys/param.h>
#include "new.h"


JMP_BUF jmpbuffer;
JMP_BUF restart_buffer;
int servernum=0;
int sd;
pid_t pgrp;
int Child=0;
int Alone=0;
int csd = -1;

#ifndef NO_PASS
char donemsg[]="DONE";
ChildInfo *Children;
int num_children = 0;
#endif

void usage(char *bin) {
    fprintf(stderr,"Usage: %s [-d directory] [-f file] [-v]\n",bin);
    fprintf(stderr,"-d directory : specify an alternate initial ServerRoot\n");
    fprintf(stderr,"-f file : specify an alternate ServerConfigFile\n");
    exit(1);
}

void htexit(int status, FILE *out) {
    fflush(out);
#if defined(NeXT) || defined(__mc68000__)
    if(standalone) longjmp(jmpbuffer,1);
#else
    if(standalone) siglongjmp(jmpbuffer,1);
#endif
	else exit(status);
}


void detach() {
    int x;

    chdir("/");
    if((x = fork()) > 0)
        exit(0);
    else if(x == -1) {
        fprintf(stderr,"httpd: unable to fork new process\n");
        perror("fork");
        exit(1);
    }
#ifndef NO_SETSID
    if((pgrp=setsid()) == -1) {
        fprintf(stderr,"httpd: setsid failed\n");
        perror("setsid");
        exit(1);
    }
#else
    if((pgrp=setpgrp(getpid(),0)) == -1) {
        fprintf(stderr,"httpd: setpgrp failed\n");
        perror("setpgrp");
        exit(1);
    }
#endif    
}

void sig_term() {
    log_error("httpd: caught SIGTERM, shutting down");
#ifndef NO_KILLPG
    killpg(pgrp,SIGKILL);
#else
    kill(-pgrp,SIGKILL);
#endif
    shutdown(sd,2);
    close(sd);
}

#ifdef BSD
void ign() {
#ifndef NeXT
    int status;
#else
    union wait status;
#endif
    pid_t pid;

    while( (pid = wait3(&status, WNOHANG, NULL)) > 0);
}
#endif

void bus_error() {
    log_error("httpd: caught SIGBUS, dumping core");
    close_logs();
    chdir(server_root);
    abort();         
    exit(1);
}

void seg_fault() {
    log_error("httpd: caught SIGSEGV, dumping core");
    close_logs();
    chdir(server_root);
    abort();
    exit(1);
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
  
void set_group_privs()
{
  struct passwd *pwent;
  int tmp_stand;
  
  if(!getuid()) {
    /* Change standalone so that on error, we die, instead of siglongjmp */
    tmp_stand = standalone;
    standalone = 0;

    if ((pwent = getpwuid(user_id)) == NULL)
      die(CONF_ERROR,"couldn't determine user name from uid",
          stdout);
    
    /* Reset `groups' attributes. */
    
    if (initgroups(pwent->pw_name, group_id) == -1)
      die(CONF_ERROR,"unable to setgroups",stdout);

    if (setgid(group_id) == -1)
      die(CONF_ERROR,"unable to change gid",stdout);

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
  
void speed_hack_libs() {
   time_t dummy_time_t = time(NULL);
   struct tm *dummy_time = localtime (&dummy_time_t);
   struct tm *other_dummy_time = gmtime (&dummy_time_t);
   char buf[MAX_STRING_LEN];
   
   strftime (buf, MAX_STRING_LEN, "%d/%b/%Y:%H:%M:%S", dummy_time);
}
 


void restart() {
#ifndef NO_PASS
    int x;
#endif

    if (Child) {
      if (!Alone) {
#if defined(NeXT) || defined(__mc68000__)
      longjmp(jmpbuffer,1);
#else
      siglongjmp(jmpbuffer,1);
#endif
      } else exit(0);
    } else {
      log_error_noclose("httpd: caught SIGHUP, restarting");
      kill_mime();
      kill_security();
      kill_indexing();

#ifndef NO_PASS
      for(x=0;x<num_children;x++) {
	close(Children[x].parentfd);
	close(Children[x].childfd);
        kill(Children[x].pid,SIGKILL);
      }
      free(Children);
#endif 
      reset_error();

      if(server_hostname) {
          free(server_hostname);
          server_hostname = NULL;
      }
      read_config(error_log);
      close_logs();
      open_logs();
      set_group_privs();
      log_error_noclose("httpd: successful restart");
      get_local_host();
      set_signals();
#if defined(NeXT) || defined(__mc68000__)      
      longjmp(restart_buffer,1);
#else
      siglongjmp(restart_buffer,1);
#endif
    }
}

void set_signals() {
    signal(SIGSEGV,(void (*)())seg_fault);
    signal(SIGBUS,(void (*)())bus_error);
    signal(SIGTERM,(void (*)())sig_term);
    signal(SIGHUP,(void (*)())restart);

#ifdef BSD
    signal(SIGCHLD,(void (*)())ign);
#else
    signal(SIGCHLD,SIG_IGN);
#endif
}

void child_alone(int csd, 
#if defined(NeXT) || defined(LINUX) || defined(SOLARIS2) || defined (__bsdi__) || defined(AIX4)
    struct sockaddr_in *sa_server,
    struct sockaddr *sa_client)
#else
    struct sockaddr_in *sa_server,
    struct sockaddr_in *sa_client)
#endif
{

/*    struct passwd* pwent; */

    Child = 1; 
    Alone = 1;
    standalone = 0;
    servernum = 0;
/* Only try to switch if we're running as root */
    if(!getuid()) {
       /* Now, make absolutely certain we don't have any privileges
        * except those mentioned in the configuration file. */

       /* Reset `groups' attribute. */
	    
       /* Note the order, first setgid() and then setuid(), it
        * wouldn't work the other way around. */

	/* all of that removed, since group already set correctly */

       if (setuid(user_id) == -1)
		die(CONF_ERROR,"unable to change uid",stdout);
    }
    ErrorStat = 0;
    status = 200;

    initialize_request();

    /* this should check error status, but it's not crucial */
    close(0);
    close(1);	
    dup2(csd,0);
    dup2(csd,1);
    
    remote_logname = (!do_rfc931 ? NULL :
		      rfc931((struct sockaddr_in *)sa_client,
			     sa_server));
    
    process_request(0,stdout);
    fflush(stdout);
    shutdown(csd,2);
    close(csd);
    exit(0);
}

#ifndef NO_PASS
void child_main(int parent_pipe, struct sockaddr_in *sa_server) {
    int x;
    
/*    struct passwd* pwent; */
    

    /* Only try to switch if we're running as root */
    if(!getuid()) {
    /* set setstandalone to 0 so we die on error, and not sigjmp */
	standalone = 0;
       /* Now, make absolutely certain we don't have any privileges
        * except those mentioned in the configuration file. */
	    
       if (setuid(user_id) == -1)
		die(CONF_ERROR,"unable to change uid",stdout);
      standalone = 1;
    } 


    for(x=0;x<num_children;x++) {
      if (parent_pipe != Children[x].parentfd) close(Children[x].parentfd);
      if (parent_pipe != Children[x].childfd) close(Children[x].childfd);
    }
  
    free(Children);

#if defined(NeXT) || defined(__mc68000__)
    if (setjmp(jmpbuffer) != 0) {
#else
    if (sigsetjmp(jmpbuffer,1) != 0) {
#endif
	fflush(stdout);
	shutdown(csd,2);
	close(csd);
	write(parent_pipe,donemsg,sizeof(donemsg));
    }
    while (1) {
	alarm(0);
	initialize_request();
	dup2(parent_pipe,0);
	dup2(parent_pipe,1);
	if ((csd = recv_fd(parent_pipe)) < 0) {
	  log_error("child error: recv_fd()");
	  close(0);
   	  close(1);
	  close(sd);
	  close(parent_pipe);
	  exit(1);
	}

   	close(0);
	close(1);	
	dup2(csd,0);
	dup2(csd,1);
	
        if (do_rfc931) {
#if defined(NeXT) || defined(LINUX) || defined(SOLARIS2) || defined(__bsdi__) || defined(AIX4)
            struct sockaddr sa_client;
#else
            struct sockaddr_in sa_client;
#endif
            int addrlen;

            addrlen = sizeof sa_client;
            getpeername(csd, &sa_client, &addrlen);
            remote_logname = rfc931((struct sockaddr_in *) &sa_client,
                                    sa_server);
        } else
            remote_logname = NULL;
	
	process_request(0,stdout);
	fflush(stdout);
	shutdown(csd,2);
	close(csd);
	write(parent_pipe,donemsg,sizeof(donemsg));
    }    
}

int make_child(int argc, char **argv, int childnum, 
		struct sockaddr_in *sa_server) {

    int fd[2];
    int pid;
    char namestr[30];

    pid = 1;
    servernum = childnum;
#ifndef NEED_SPIPE
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == -1) { 
#else
    if (s_pipe(fd)  == -1) {
#endif
	log_error("Unable to open socket for new process");
	return -1;
     } else { 
      if ((pid = fork()) == -1) {
	log_error("Unable to fork new process");
	close(fd[1]);
	close(fd[0]);
      } else {
	Children[childnum].childfd = fd[1];
	Children[childnum].parentfd = fd[0];
	Children[childnum].pid = pid;
	Children[childnum].busy = 0;
      }
    
      if (!pid) {
	close(Children[childnum].childfd);
#ifdef BSD
        signal(SIGCHLD,(void (*)())ign);
#else
	signal(SIGCHLD,SIG_IGN);
#endif
	sprintf(namestr,"httpd-child%d",childnum);
	inststr(argv,argc,namestr);
	Child = 1;
	child_main(Children[childnum].parentfd, sa_server);
      } else {
	close(Children[childnum].parentfd);
      } /* if child else */
   } /* if error on open else */
   return childnum;
}
#endif /* NO_PASS */

void standalone_main(int argc, char **argv) {
    char msg[10];
    fd_set listen_set;
    int csd, clen,pid;
    int x,error,num_sigs,nread;
    int max, free_child;
    static int last_child = 0;
    int    search_cnt;
    int keepalive_value = 1;  
    int one = 1;
    struct sockaddr_in sa_server;
#if defined(NeXT) || defined(LINUX) || defined(SOLARIS2) || defined(__bsdi__) || defined(AIX4)
    struct sockaddr sa_client;
#else
    struct sockaddr_in sa_client;
#endif


   /* Some systems need to call TZSET before detaching from the shell
      process to ensure proper time zone settings */
#ifdef CALL_TZSET
    tzset();
#endif 

    detach();  
    inststr(argv, argc, "httpd-root");

    if ((sd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP)) == -1) {
        fprintf(stderr,"httpd: could not get socket\n");
        perror("socket");
        exit(1);
    }

    if((setsockopt(sd,SOL_SOCKET,SO_REUSEADDR,(char *)&one,sizeof(one))) == -1) {
        fprintf(stderr,"httpd: could not set socket option SO_REUSEADDR\n");
        perror("setsockopt");
        exit(1);
    }

/* Added at the suggestion of SGI, seems some PC clients don't close the 
   connection, which causes IRIX and Solaris to keep sending them packets
   for a really long time . . . 
   Sent in From: Brian Behlendorf <brian@wired.com> */

    if((setsockopt(sd,SOL_SOCKET,SO_KEEPALIVE,(void *)&keepalive_value,
        sizeof(keepalive_value))) == -1) {
        fprintf(stderr,"httpd: could not set socket option SO_KEEPALIVE\n"); 
        perror("setsockopt"); 
        exit(1); 
    }

    bzero((char *) &sa_server, sizeof(sa_server));
    sa_server.sin_family=AF_INET;
    sa_server.sin_addr.s_addr=htonl(INADDR_ANY);
    sa_server.sin_port=htons(port);
    if(bind(sd,(struct sockaddr *) &sa_server,sizeof(sa_server)) == -1) {
        fprintf(stderr,"httpd: could not bind to port %d\n",port);
        perror("bind");
        exit(1);
    }
    listen(sd,35);

    log_pid();

#if defined(NeXT) || defined(__mc68000__)
    setjmp(restart_buffer);
#else
    sigsetjmp(restart_buffer,1);
#endif

    set_signals();
    speed_hack_libs();

    pid = 1;
    error = 0;

#ifndef NO_PASS
    num_children = 0;
    Children = (ChildInfo *) malloc(sizeof(ChildInfo)*(max_servers+1));
    while (num_children < start_servers) {
	make_child(argc, argv, num_children++, &sa_server);
    }
#endif

    while(1) {
	FD_ZERO(&listen_set);

	FD_SET(sd,&listen_set);
	max = sd;

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
			log_error("child error: read msg failure");
		    } else if (nread == 0) {
			log_error("child error: child connection closed");
			close(Children[x].childfd);
			kill(Children[x].pid,SIGKILL);
			make_child(argc, argv, x, &sa_server);
		    } else {
#ifdef DEBUG_BCL
			fprintf(stderr,"-%d ",x); 
#endif
			if (!strcmp(msg,donemsg)) {
			    Children[x].busy = 0;
			} /* if strcmp */
		    } /* if nread else */
		} /* if FD_ISSET */
	    } /* for x to num_children */
#endif /* NO_PASS */
	    if (FD_ISSET(sd, &listen_set)) {
		clen=sizeof(sa_client);
		if((csd=accept(sd,&sa_client,&clen)) == -1) {
		    if (errno == EINTR) {
#ifdef BSD			  
			ign();
#endif
		    } else {
			log_error("socket error: accept failed");
		    }
		} else { /* connection accepted */
#ifndef NO_PASS
		  if (num_children) {
		    /*free_child = 0;*/
		    search_cnt = 0;
		    free_child = last_child;
		    while (Children[free_child].busy) {
			free_child = (free_child + 1) % num_children; 
			search_cnt++;
		        /* free_child++ */
			if (free_child == last_child) 
			    break;
		    }
		    if (search_cnt >= num_children) {
			if ( num_children < max_servers) {
			    if (make_child(argc, argv, num_children, &sa_server) >= 0) {
			      free_child=num_children;
			      Children[free_child].busy = 1;
			      if (pass_fd(Children[free_child].childfd,csd) < 0)
				log_error("child error: pass failed");
			      last_child = 0;
			      num_children++;
			    } else {
			      log_error("Main error: make_child failed");
		            } /* if (make_child) else ... */
			} else { 
/* Already have as many children as compiled for.*/
			    if (!fork()) {
				inststr(argv, argc, "httpd-alone");
				child_alone(csd,&sa_server,&sa_client);
			    } /* fork */
			} /* if (num_children < max_servers) ... else ... */
		    } else {
			Children[free_child].busy = 1;
			last_child = (free_child + 1) % num_children;
			if (pass_fd(Children[free_child].childfd,csd) < 0)
			    log_error("child error: pass failed");
		    } /* if (search_cnt >= num_children) ... else ... */
		   close(csd);
                  } else 
#endif /* NO_PASS */
		  {
		     if (!fork()) {
			inststr(argv, argc, "httpd-alone");
			child_alone(csd,&sa_server,&sa_client);
		      } /* fork */
		      close(csd);
		  } /* if (num_children) ... else ... */
		} /* If good accept */
	    } /* if sd ready for read */
	} /* if select */
    } /* while 1 */
} /* standalone_main */

extern char *optarg;
extern int optind;

main(int argc, char *argv[])
{
    int c;

    strcpy(server_root,HTTPD_ROOT);
    make_full_path(server_root,SERVER_CONFIG_FILE,server_confname);

    while((c = getopt(argc,argv,"d:f:v")) != -1) {
        switch(c) {
          case 'd':
            strcpy(server_root,optarg);
	  make_full_path(server_root,SERVER_CONFIG_FILE,server_confname);
            break;
          case 'f':
            strcpy(server_confname,optarg);
            break;
          case 'v':
            printf("NCSA httpd version %s.\n",SERVER_VERSION);
            exit(1);
          case '?':
            usage(argv[0]);
        }
    }
    read_config(stderr);
    open_logs();
    set_group_privs();
    get_local_host();

#ifdef __QNX__
    dup2(0,1);
    dup2(0,2);
#endif

    if(standalone)
        standalone_main(argc,argv);
    else {
	initialize_request();
        user_id = getuid();
        group_id = getgid();

        port = get_portnum(fileno(stdout),stdout);
        if(do_rfc931)
            remote_logname = get_remote_logname(stdout);
        process_request(0,stdout);
	fflush(stdout);
    }
    close_logs();
    fclose(stdin);
    fclose(stdout);
    exit(0);
}
