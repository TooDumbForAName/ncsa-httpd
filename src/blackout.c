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
 * blackout.c,v 1.3 1996/03/07 21:27:21 blong Exp
 *
 ************************************************************************
 *
 */


#include "config.h"
#include "portability.h"

#include <stdio.h>
#ifndef NO_STDLIB_H
# include <stdlib.h>
#endif /* NO_STDLIB_H */
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <setjmp.h>
#include <ctype.h>
#include "constants.h"
#include "fdwrap.h"
#include "allocate.h"
#include "http_send.h"
#include "cgi.h"
#include "imagemap.h"
#include "http_mime.h"
#include "http_log.h"
#include "http_request.h"
#include "http_config.h"
#include "http_include.h"
#include "http_alias.h"
#include "http_access.h"
#include "http_dir.h"
#include "httpd.h"
#include "util.h"

#ifdef BLACKOUT_CODE

char *bodyTag = "<BODY BGCOLOR=\"#000000\" TEXT=\"#FFFFFF\" LINK=\"#0099FF\" VLINK=\"#00FF99\"\n>";

static void (*exit_callback)(void);


int realWrite(int fd, char *buf, int length)
{
   int left = length;
   int sent = 0;
   int off = 0;
   int wrote = 0;

   while(left) {
     if ((wrote = write(fd,&buf[off],left)) < 0) {
       if (errno != EINTR) break;
     }
     left -= wrote;
     off += wrote;
     sent += wrote;
   }

   return sent;
} 

int sendBody(per_request *reqInfo, char *buf, int length) 
{
  int x = 0;
  int found = 0;
  int state = 0;
  int begin = 0;

  while(x < length) {
    switch (state) {
      case 0 : 
         if (buf[x] == '<') {
	   begin = x;
	   state++;
         }
	 break;
      case 1 :
	 if ((buf[x] == 'B') || (buf[x] == 'b')) state++;
	  else state = 0;
         break;
      case 2 :
	 if ((buf[x] == 'O') || (buf[x] == 'o')) state++;
	  else state = 0;
         break;
      case 3 :
	 if ((buf[x] == 'D') || (buf[x] == 'd')) state++;
	  else state = 0;
         break;
      case 4 :
	 if ((buf[x] == 'Y') || (buf[x] == 'y')) state++;
	  else state = 0;
         break;
/*      case 5 :
	 if (buf[x] == ' ') state++;
	  else state = 0;
         break; */
      case 5 : 
	 if (buf[x] == '>') state++;
	 break;
      case 6 :
	 realWrite(fileno(reqInfo->out), buf,begin);
	 realWrite(fileno(reqInfo->out), bodyTag, strlen(bodyTag));
	 realWrite(fileno(reqInfo->out), buf+x,length-x);
	 x = length;
    }
    x++;
  }

  if (state != 6) realWrite(fileno(reqInfo->out),buf,length);
  return FALSE;
}

/*
  We'll make it return the number of bytes sent
  so that we know if we need to send a body by default
*/
long send_fp_black(per_request *reqInfo, FILE *f, void (*onexit)(void))
{
    char *buf;
    long total_bytes_sent;
    register int n,o,w;
    int isHTML = FALSE;

    buf = newString(IOBUFSIZE,STR_TMP);
    exit_callback = onexit;
    signal(SIGALRM,send_fd_timed_out);
    signal(SIGPIPE,send_fd_timed_out);

    total_bytes_sent = 0;
    if (!strcmp(reqInfo->outh_content_type,"text/html")) {
      isHTML = TRUE;
      total_bytes_sent = rprintf(reqInfo,bodyTag);
    }
    rflush(reqInfo);
    while (1) {
        alarm(timeout);
        if((n=fread(buf,sizeof(char),IOBUFSIZE,f)) < 1) {
	   if (errno != EINTR) break;
        }

        o=0;
        if(reqInfo->bytes_sent != -1)
            reqInfo->bytes_sent += n;
        if (isHTML) {
	    sendBody(reqInfo,buf,n);
	    total_bytes_sent += n;
	    n = 0;
          }
        while(n) {
/*   Seems some systems have broken fwrite's (like AIX 3.2.5 on PowerPC)
 *   this should be a drop in replacement, maybe even be faster.
 *   For now, we'll just replace, but may have to #define one or the other
 *   depending on the system.
 */
	    if ((w=write(fileno(reqInfo->out),&buf[o],n)) < 0) {
	      if (errno != EINTR) break;
	    }
            n-=w;
            o+=w;
	    total_bytes_sent += w;
        }

    }
    if (isHTML) 
    rprintf(reqInfo,"<HR><a href=\"http://www.vtw.org/speech/\">My World Wide Web Pages are black for 48 hours to protest second-class treatment from the US Government for free speech.  Read about it at this WWW page.</a>");
    alarm(0);
    signal(SIGALRM,SIG_IGN);
    signal(SIGPIPE,SIG_IGN);
    freeString(buf);
    return total_bytes_sent;
}

#endif /* BLACKOUT_CODE */
