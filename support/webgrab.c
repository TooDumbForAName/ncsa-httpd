/***************************************************************************\
 *  webgrab - v1.0 - Copyright 1995, Brian J. Swetland                       *
 *                                                                           *
 *  Free for any personal or non-comercial use.  Use at your own risk.       *
\***************************************************************************/

#include <stdio.h>
#include <fcntl.h>

#ifdef __bsdi__
# include <sys/malloc.h>
#else
# ifndef NeXT
#  include <malloc.h>
# endif
#endif

#include <sys/time.h>
#include <sys/types.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>

#include <string.h>

#define VERSION "1.3"

/* strdup isn't portable, so we make our own.  */
char *strd(char *s) {
    char *d;
 
    d=(char *)malloc(strlen(s) + 1);
    strcpy(d,s);
    return(d);
}

/* parses URL looking like blah://host[:port][/path]
   will ignore anything before the first : and terminate path when it
   hits >, ", or whitespace -- returns portno or 0 if bad url */

int parseURL(char *url, char **host, char **path)
{
  char *p, *pp;
  int port;
  
  p = url;  
  
  /* skip anything up to the first : (the one after http, etc) */
  while(*p && *p!=':') p++;
  if(!*p) 
    return 0;
  
  /* REQUIRE two '/'s */
  if(!(*(++p) && (*p =='/') && *(++p) && (*p == '/')))
    return 0;
  
  p++;
  
  /* mark the beginning of the hostname */
  pp = p;
  /* hostname is terminated by a '/' or '>','"',or whitespace */
  while(*p && *p!=':' && *p!='/' && *p!='"' && *p!='>' && !isspace(*p)) 
    p++;
  
  *host = (char *) malloc(p-pp+1);
  strncpy(*host,pp,p-pp);
  (*host)[p-pp]=0;
  
  /* optionally read a portnumber */
  if(*p==':'){
    p++;
    port = 0;
    while(*p && isdigit(*p)){
      port = port*10 + (*p-'0');
      p++;
    }
    if(!*p || *p!='/') {
      free(*host);
      return 0;
    }
  } else {
    port = 80;
  }
  
  /* still more */
  if(*p && (*p=='/')){
    pp = p;
    while(*p && *p!='"' && *p!='>' && !isspace(*p)) p++;
    *p = 0;
    *path = strd(pp);
  } else {
    *path = strd("/");
  }
  return port;
}

void usage(char *argv) 
{
  printf("\nWebgrab: The Command Line Browser\tVersion %s \n",VERSION);
  printf("Usage: %s [-shr] <url>\n",argv);
  printf("\t-s\t: Suppress Headers\n");
  printf("\t-h\t: Headers Only\n");
  printf("\t<url>\t: URL to retrieve (in http:// format)\n");
  printf("\t-r\t: Read HTTP headers from stdin\n\n");
  exit(1);
}

int main(int argc, char *argv[])
{
  int s, i, port;
  struct sockaddr_in sa;
  struct hostent *hp;
  FILE *fpo,*fpi;
  char buf[1024];
  char *path,*host;
  int ignore=0,head=0,readin=0;
  
  if(argc<2 || (argv[1][0]=='-'&&argc<3)){
    usage(argv[0]);
  }
  
  if(argv[1][0]=='-'){
    for(path=&argv[1][1];*path;path++)
      switch(*path){
      case 'r':
	readin = 1;
	break;
      case 's':
	ignore = 1;
	break;
      case 'h':
	head = 1;
	break;
      }
    s = 2;
  } else {
    s = 1;
  }
  
  if(!(port=parseURL(argv[s], &host, &path))){
    fprintf(stderr,"error: invalid url\n");
    exit(1);
  }
  
  /* find the server */
  if(!(hp = gethostbyname(host))) {
    fprintf(stderr,"error: can't get host %s.\n",host);
    exit(1);
  }
  
  /* Setup the socket */
/*  bzero((char *)&sa, sizeof(sa)); */
  memset(&sa, 0, sizeof(sa));
  sa.sin_port = htons(port);
/*  bcopy((char *)hp->h_addr, (char *)&sa.sin_addr, hp->h_length); */
  memcpy((char *)&sa.sin_addr, (char *)hp->h_addr, hp->h_length);
  sa.sin_family = hp->h_addrtype;
  
  /* allocate the socket */
  if((s = socket(hp->h_addrtype, SOCK_STREAM, 0)) < 0){
    fprintf(stderr,"error: can't get socket\n");
    exit(1);
  }
  
  /* connect to the server */
  if(connect(s, &sa, sizeof(sa)) < 0){
    close(s);
    fprintf(stderr,"error: can't connect\n");
    exit(1);
  }
  
  fpo = fdopen(s,"w");
  fpi = fdopen(s,"r");
  fprintf(fpo,"%s %s HTTP/1.0\r\n",head?"HEAD":"GET",path);
  if (readin) {
    /* copy headers from stdin ... */
    while(!feof(stdin)){
      i = fread(buf,1,1024,stdin);
      if(i) fwrite(buf,1,i,fpo);
        if(feof(stdin)) break;
    } 
  } else {
    /* send our normal header info */
    fputs("User-Agent: WebGrab/1.2 (commandline forever)\r\n",fpo);
  }      
  fputs("\r\n",fpo);
  fflush(fpo);
  
  /* IGNORE HEADERS */
  while(!feof(fpi)){
    fgets(buf,1024,fpi);
    if(!ignore) fprintf(stdout,"%s",buf);
    if(feof(fpi) || buf[0]<' ') break;
  }
  while(!feof(fpi)){
    i = fread(buf,1,1024,fpi);
    if(i) fwrite(buf,1,i,stdout);
    if(feof(fpi)) break;
  }
  close(s);
  exit(0);
}

