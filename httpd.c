/*
 * httpd.c: simple http daemon for answering WWW file requests
 *
 * 
 * Rob McCool 3/21/93
 * 
 */


#include "httpd.h"
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

void usage(char *bin) {
    fprintf(stderr,"Usage: %s [-s] [-f filename] [-p portnum] [-u userid] [-g groupid]\n",bin);
    fprintf(stderr,"-f filename : specify an alternate config file\n");
    fprintf(stderr,"-s : Run as standalone server instead of from inetd\n");
    fprintf(stderr,"-p portnumber : alternate port number.\n");
    fprintf(stderr,"(server must initially run as root for ports under 1024.)\n");
    fprintf(stderr,"\nThe following two options require the daemon to be initially run as root:\n");
    fprintf(stderr,"-u userid : run as named user-id\n");
    fprintf(stderr,"-g groupid : run as named group-id\n");
    fprintf(stderr,"A user or group number may be specified with a # as the first character.\n");
    exit(1);
}

void detach() {
    int x;

    if((x = fork()) > 0)
        exit(0);
    else if(x == -1) 
        server_error(stdout,FORK);

#ifdef BSD
    setpgrp(0,getpid());
    if((x=open("/dev/tty",O_RDWR,0)) >= 0) {
        ioctl(x,TIOCNOTTY,0);
        close(x);
        setsid();
    }
#else
    setpgrp();
#endif
}

#ifdef BSD
void ign() {
    int status;
    pid_t pid;

    while( (pid = wait3(&status, WNOHANG, NULL)) > 0);
}
#endif

void standalone_main(int port, uid_t uid, gid_t gid) {
    int sd,csd, clen,pid;
    struct sockaddr_in sa_server,sa_client;

    detach();

    if ((sd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP)) == -1)
        server_error(stdout,SOCKET);
    
    bzero((char *) &sa_server, sizeof(sa_server));
    sa_server.sin_family=AF_INET;
    sa_server.sin_addr.s_addr=htonl(INADDR_ANY);
    sa_server.sin_port=htons(port);
    if(bind(sd,(struct sockaddr *) &sa_server,sizeof(sa_server)) == -1) {
        perror("bind");
        server_error(stdout,SOCKET_BIND);
    }
    listen(sd,5);

#ifdef BSD
    signal(SIGCHLD,(void (*)())ign);
#else
    signal(SIGCHLD,SIG_IGN);
#endif

    /* we're finished getting the socket, switch to running userid */
    setuid(uid);
    setgid(gid);
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
            perror("accept");
            server_error(stdout,SOCKET_ACCEPT);
        }
        if((pid = fork()) == -1)
            server_error(stdout,FORK);
            else if(!pid) {
                close(0);
                close(1);
                dup2(csd,0);
                dup2(csd,1);
                close(sd);
                process_request(stdin,stdout);
                fclose(stdin);
                fclose(stdout);
                exit(0);
            }
        close(csd);
    }
}

extern char *optarg;
extern int optind;

main(int argc, char *argv[])
{
    char config_file[MAX_STRING_LEN];
    int standalone=0,c,port = HTTPD_PORT;
    uid_t uid;
    gid_t gid;

    uid = uname2id(DEFAULT_USER);
    gid = gname2id(DEFAULT_GROUP);

    strcpy(config_file,CONFIG_FILE);
    while((c = getopt(argc,argv,"sf:p:u:g:")) != -1) {
        switch(c) {
          case 's':
            standalone=1;
            break;
          case 'f':
            strcpy(config_file,optarg);
            break;
          case 'p':
            port = atoi(optarg);
            break;
          case 'u':
            uid = uname2id(optarg);
            break;
          case 'g':
            gid = gname2id(optarg);
            break;
          case '?':
            usage(argv[0]);
        }
    }

    read_config(config_file,stdout);

    if(standalone)
        standalone_main(port,uid,gid);
    else
        process_request(stdin,stdout);
    fclose(stdin);
    fclose(stdout);
    exit(0);
}
