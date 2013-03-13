/*
 * httpd.c: simple http daemon for answering WWW file requests
 *
 * 
 * Rob McCool 3/21/93
 * 
 */


#include "httpd.h"

void usage(char *bin) {
    fprintf(stderr,"Usage: %s [-d directory] [-f file] [-v]\n",bin);
    fprintf(stderr,"-d directory : specify an alternate initial ServerRoot\n");
    fprintf(stderr,"-f file : specify an alternate ServerConfigFile\n");
    exit(1);
}

void htexit(int status, FILE *out) {
#ifdef PEM_AUTH
    pem_cleanup(status,out);
#endif
    exit(status);
}

int sd;
pid_t pgrp;

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
#ifndef NEXT
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
    chdir(server_root);
    abort();         
}

void seg_fault() {
    log_error("httpd: caught SIGSEGV, dumping core");
    chdir(server_root);
    abort();
}

void set_signals();

void restart() {
    log_error("httpd: caught SIGHUP, restarting");
    kill_mime();
    kill_security();
    kill_indexing();
    if(server_hostname) {
        free(server_hostname);
        server_hostname = NULL;
    }
    read_config();
    close_logs();
    open_logs();
    log_error("httpd: successful restart");
    get_local_host();
    set_signals();
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


void standalone_main() {
    int csd, clen,pid, one=1;
#ifdef NEXT
    struct sockaddr_in sa_server;
    struct sockaddr sa_client;
#else
    struct sockaddr_in sa_server,sa_client;
#endif

    detach();

    if ((sd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP)) == -1) {
        fprintf(stderr,"httpd: could not get socket\n");
        perror("socket");
        exit(1);
    }

    if((setsockopt(sd,SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one))) == -1) {
        fprintf(stderr,"httpd: could not set socket option\n");
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
    listen(sd,5);

    set_signals();
    log_pid();

    while(1) {
      retry:
        clen=sizeof(sa_client);
        if((csd=accept(sd,&sa_client,&clen)) == -1) {
            if(errno == EINTR)  {
#ifdef BSD
                ign();
#endif
                goto retry;
            }
            log_error("socket error: accept failed");
            goto retry;
        }
        remote_logname = (!do_rfc931 ? NULL :
                          rfc931((struct sockaddr_in *)&sa_client,&sa_server));

	/* we do this here so that logs can be opened as root */
        if((pid = fork()) == -1)
            log_error("unable to fork new process");
        else if(!pid) {
	    struct passwd* pwent;
            struct linger sl;

            sl.l_onoff = 1;
            sl.l_linger = 600; /* currently ignored anyway */
            /* this should check error status, but it's not crucial */
            setsockopt(csd,SOL_SOCKET,SO_LINGER,&sl,sizeof(sl));

            close(0);
            close(1);
            dup2(csd,0);
            dup2(csd,1);
            close(sd);
            /* Only try to switch if we're running as root */
            if(!getuid()) {
                /* Now, make absolutely certain we don't have any privileges
                 * except those mentioned in the configuration file. */
                if ((pwent = getpwuid(user_id)) == NULL)
                    die(SERVER_ERROR,"couldn't determine user name from uid",
                        stdout);
                /* Reset `groups' attribute. */
                if (initgroups(pwent->pw_name, group_id) == -1)
                    die(SERVER_ERROR,"unable to setgroups",stdout);

                /* Note the order, first setgid() and then setuid(), it
                 * wouldn't work the other way around. */
                if (setgid(group_id) == -1)
                    die(SERVER_ERROR,"unable to change gid",stdout);
                if (setuid(user_id) == -1)
                    die(SERVER_ERROR,"unable to change uid",stdout);
            }
            process_request(0,stdout);
            fclose(stdin);
            fclose(stdout);
            shutdown(csd,2);
            close(csd);
            exit(0);
        }
        close(csd);
    }
}

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
    read_config();
    open_logs();
    get_local_host();

    if(standalone)
        standalone_main();
    else {
        user_id = getuid();
        group_id = getgid();
        port = get_portnum(fileno(stdout),stdout);
        if(do_rfc931)
            remote_logname = get_remote_logname(stdout);
        process_request(0,stdout);
    }
    fclose(stdin);
    fclose(stdout);
    exit(0);
}
