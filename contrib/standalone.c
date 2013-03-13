/* Main program for httpd - allows operation as a standalone program
 * without running from inetd or as an inetd child.
 */
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <signal.h>

extern int getopt(int argc, char ** argv, const char * optstring);
extern char *optarg;
extern int optind, opterr;

#include "httpd.h"

void
DoNothing()
{
   return;
}

void
SetAlarm()
{
   alarm(10);
}

int
main(int argc, char ** argv)
{
   int srvsock;
   struct sockaddr_in iso;
   int srvport;
   char * progname;
   char * config_file=CONFIG_FILE;
   char * portname = "http";
   int standalone = 0;
   int inetd = 0;
   int badarg = 0;
   int c;
   char * errptr;
   struct sigaction sa;

   memset((void *)&sa, 0, sizeof(struct sigaction));
   sa.sa_handler = DoNothing;
   sigfillset(&sa.sa_mask);
   sa.sa_flags = 0;
   sigaction(SIGCLD, &sa, NULL);
   sa.sa_handler = SetAlarm;
   sigaction(SIGALRM, &sa, NULL);
   alarm(10);
   errno = 0;
   progname = strrchr(argv[0], '/');
   if (progname == NULL) {
      progname = argv[0];
   } else {
      ++progname;
   }
   while ((c = getopt(argc, argv, "Hf:p:si")) != EOF) {
      switch(c){
      case 'f':
         config_file = optarg;
         break;
      case 'H':
         fprintf(stderr,"\
%s - a simple http daemon\n\
Usage: %s [-H] [-f file] [-p port] [-s] [-i]\n\
\n\
-H	print this message and exit\n\
-f file	specify name of config file (default is %s)\n\
-p port	specify symbolic or numeric port to listen on (default is http)\n\
-s	act as a standalone server rather than run under inetd\n\
-i	assume startup was from inetd.\n\
\n\
Note that one of the -i or -s options must be given.\n",
            progname, progname, CONFIG_FILE);
         exit(0);
         break;
      case 'p':
         portname = optarg;
         break;
      case 's':
         standalone = 1;
         inetd = 0;
         break;
      case 'i':
         inetd = 1;
         standalone = 0;
         break;
      default:
         badarg = 1;
         break;
      }
   }
   argc -= optind;
   argv += optind;
   if (argc > 0) {
      badarg = 1;
   }
   if (badarg) {
      fprintf(stderr, "%s: incorrect arguments, use -H option for help.\n",
         progname);
      exit(2);
   }
   if (standalone == 0 && inetd == 0) {
      fprintf(stderr,
         "%s: must give one of -i or -s options (see -H for help).\n",
         progname);
      exit(2);
   }
   if (inetd) {
      /* If this is supposed to run from inetd, just start processing requests.
       */
      read_config(config_file,stdout);
      process_request(stdin,stdout);

      fclose(stdin);
      fclose(stdout);
      exit(0);
   }

   /* This must be a standalone server - do the inetd work myself, forking
    * off a child to handle each new connection.
    */

   srvport = (int)strtol(portname, &errptr, 10);
   if (errptr == NULL || errptr == portname || *errptr != '\0') {
      struct servent *sp;
      sp = getservbyname(portname, "tcp");
      if (sp == NULL) {
         fprintf(stderr, "%s: unable to lookup service \"%s\".\n",
            progname, portname);
         exit(2);
      }
      srvport = sp->s_port;
   }
   srvsock = socket(AF_INET, SOCK_STREAM, 0);
   if (srvsock < 0) {
      fprintf(stderr, "%s: socket() call failed with errno %d (%s)\n",
         progname, errno, strerror(errno));
      exit(2);
   }
   memset(&iso, 0, sizeof(iso));
   iso.sin_family = AF_INET;
   iso.sin_addr.s_addr = INADDR_ANY;
   iso.sin_port = srvport;
   errno = 0;
   if (bind(srvsock, (struct sockaddr *)&iso, sizeof(iso)) == -1) {
      fprintf(stderr, "%s: bind() call failed with errno %d (%s)\n",
         progname, errno, strerror(errno));
      exit(2);
   }
   errno = 0;
   if (listen(srvsock, 5) == -1) {
      fprintf(stderr, "%s: listen() call failed with errno %d (%s)\n",
         progname, errno, strerror(errno));
      exit(2);
   }
   for ( ; ; ) {
      struct sockaddr_in fromaddr;
      int fromlen;
      int newfd;
      int childpid;

      memset(&fromaddr, 0, sizeof(fromaddr));
      fromaddr.sin_family = AF_INET;
      fromlen = sizeof(fromaddr);
      errno = 0;
      newfd = accept(srvsock, (struct sockaddr *)&fromaddr, &fromlen);
      if (newfd < 0) {
         if (errno == EINTR) {
            /* This must have been a SIGCLD or SIGALRM awakening us from a
             * deep slumber. Just reap children to get rid of zombies and
             * loop again.
             */
            int stat;
            int rval;

            while ((rval = waitpid((pid_t)-1, &stat, WNOHANG)) > 0);
         } else {
            fprintf(stderr, "%s: accept() call failed with errno %d (%s)\n",
               progname, errno, strerror(errno));
            exit(2);
         }
      } else {
         childpid = fork();
         if (childpid < 0) {
            fprintf(stderr, "%s: fork() call failed with errno %d (%s)\n",
               progname, errno, strerror(errno));
            exit(2);
         } else if (childpid == 0) {
            /* This is the child. Set stdin and stderr to newfd and
             * process httpd type stuff. Alos clear any alarms and
             * signal handlers.
             */
            alarm(0);
            sa.sa_handler = SIG_DFL;
            sigaction(SIGALRM, &sa, NULL);
            sigaction(SIGCLD, &sa, NULL);
            close(0);
            close(1);
            dup2(newfd,0);
            dup2(newfd,1);
            close(newfd);
            setvbuf(stdout, NULL, _IOLBF, BUFSIZ);
            read_config(config_file,stderr);
            process_request(stdin,stdout);
            fclose(stdin);
            fclose(stdout);
            exit(0);
         } else {
            /* In parent, just close the new socket and listen again
             */
            close(newfd);
         }
      }
   }
}
