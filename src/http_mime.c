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
 * http_mime.c,v 1.98 1995/11/28 09:02:07 blong Exp
 *
 ************************************************************************
 *
 * http_mime.c: Sends/gets MIME headers for requests
 * 
 * Based on NCSA HTTPd 1.3 by Rob McCool
 * 
 * 03/19/95 blong
 *      Added set_stat_line as part of making user config error messages work
 *      The correct status line should now be sent back
 *
 * 04/20/95 blong
 *      Added a modified "B18" from apache patches by Rob Hartill
 *
 * 08/07/95 blong
 *      Moved scan_script_header() function to cgi.c in an attempt at
 *      increased modularity of the code (at least representatively)
 *
 * 10/30/95 blong
 *	Fixed get_mime_header string length problems as suggested by
 *      Marc Evans (Marc@Destek.NET)
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
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include "constants.h"
#include "fdwrap.h"
#include "http_mime.h"
#include "http_log.h"
#include "http_config.h"
#include "http_access.h"
#include "env.h"
#include "util.h"

#if defined(KRB4) || defined(KRB5)
#define HAVE_KERBEROS
#endif /* defined(KRB4) || defined(KRB5) */

#if 1
#define hash(i) (isalpha(i) ? (tolower(i)) - 'a' : 26)
#else
#define hash(i) ((i) % 27)
#endif /* 1 */

/* Hash table */
struct mime_ext *types[27];
struct mime_ext *forced_types;
struct mime_ext *encoding_types;
struct mime_ext *Saved_Forced;
struct mime_ext *Saved_Encoding;

int content_length;
char content_type[MAX_STRING_LEN];
char content_type_in[MAX_STRING_LEN];
char content_encoding[MAX_STRING_LEN];

char location[MAX_STRING_LEN];
static char last_modified[MAX_STRING_LEN];

char auth_line[HUGE_STRING_LEN];

char called_hostname[MAX_STRING_LEN];

char *out_headers = NULL;
char *status_line = NULL;
char ims[MAX_STRING_LEN]; /* If-modified-since */

#ifdef HAVE_KERBEROS
char out_auth_header[1024];
#endif /* HAVE_KERBEROS */


void hash_insert(struct mime_ext *me) {
    register int i = hash(me->ext[0]);
    register struct mime_ext *p, *q;

    if(!(q=types[i])) {
        types[i]=me;
        return;
    }
    if((!(p=q->next)) && (strcmp(q->ext,me->ext) >= 0)) {
        types[i]=me;
        me->next=q;
        return;
    }
    while(p) {
        if(strcmp(p->ext,me->ext) >= 0) break;
        q=p;
        p=p->next;
    }
    me->next=p;
    q->next=me;
}

void kill_mime(void) 
{
    register struct mime_ext *p,*q;
    register int x;

    for(x=0;x<27;x++) {
        p=types[x];
        while(p) {
            free(p->ext);
            free(p->ct);
            q=p;
            p=p->next;
            free(q);
        }
    }
    p=forced_types;
    while(p) {
        free(p->ext);
        free(p->ct);
        q=p;
        p=p->next;
        free(q);
    }
    p=encoding_types;
    while(p) {
        free(p->ext);
        free(p->ct);
        q=p;
        p=p->next;
        free(q);
    }
}

void init_mime(void) 
{
    char l[MAX_STRING_LEN],w[MAX_STRING_LEN],*ct;
    FILE *f;
    per_request reqInfo;
    register struct mime_ext *me;
    register int x;

    reqInfo.out = stderr;

    if(!(f = FOpen(types_confname,"r"))) {
        fprintf(stderr,"httpd: could not open mime types file %s\n",
                types_confname);
        perror("fopen");
        exit(1);
    }

    for(x=0;x<27;x++) 
        types[x] = NULL;
    forced_types = NULL;
    encoding_types = NULL;

    while(!(cfg_getline(l,MAX_STRING_LEN,f))) {
        if(l[0] == '#') continue;
        cfg_getword(w,l);
        if(!(ct = (char *)malloc(sizeof(char) * (strlen(w) + 1))))
            die(&reqInfo,SC_NO_MEMORY,"init_mime");
        strcpy(ct,w);

        while(l[0]) {
            cfg_getword(w,l);
            if(!(me = (struct mime_ext *)malloc(sizeof(struct mime_ext))))
                die(&reqInfo,SC_NO_MEMORY,"init_mime");
            if(!(me->ext = (char *)malloc(sizeof(char) * (strlen(w)+1))))
                die(&reqInfo,SC_NO_MEMORY,"init_mime");
            for(x=0;w[x];x++)
                me->ext[x] = (islower(w[x]) ? w[x] : tolower(w[x]));
            me->ext[x] = '\0';
            if(!(me->ct=strdup(ct)))
                die(&reqInfo,SC_NO_MEMORY,"init_mime");
            me->next=NULL;
            hash_insert(me);
        }
        free(ct);
    }
    FClose(f);
}

#ifdef DEBUG
void dump_types(void) 
{
    struct mime_ext *p;
    register int x;

    for(x=0;x<27;x++) {
        p=types[x];
        while(p) {
            printf("ext %s: %s\n",p->ext,p->ct);
            p=p->next;
        }
    }
    p=forced_types;
    while(p) {
        printf("file %s: %s\n",p->ext,p->ct);
        p=p->next;
    }
}
#endif /* DEBUG */
int is_content_type(char *type) {
    return(!strcmp(content_type,type));
}

void find_ct(per_request *reqInfo, char *file, int store_encoding) {
    int i,l,l2;
    struct mime_ext *p;
    char fn[MAX_STRING_LEN];

    lim_strcpy(fn,file, MAX_STRING_LEN);
/*    l = strlen(fn);
    if (fn[l-1] == '/') 
      fn[l-1] = '\0'; */
    if((i=rind(fn,'.')) >= 0) {
        ++i;
        l=strlen(fn);
        p = encoding_types;

        while(p) {
            if(!strcmp(p->ext,&fn[i])) {
                fn[i-1] = '\0';
                if(store_encoding) {
                    if(content_encoding[0])
                        sprintf(content_encoding,"%s, %s",content_encoding,
                                p->ct);
                    else
                        strcpy(content_encoding,p->ct);
                }
                if((i=rind(fn,'.')) < 0)
                    break;
                ++i;
                l=strlen(fn);
                p=encoding_types;
            }
            else
                p=p->next;
        }
    }
    p=forced_types;
    l=strlen(fn);

    while(p) {
        l2=l-strlen(p->ext);
        if((l2 >= 0) && (!strcasecmp(p->ext,&fn[l2]))) {
            strcpy(content_type,p->ct);
            return;
        }
        p=p->next;
    }

    if((i = rind(fn,'.')) < 0) {
        if (local_default_type[0] != '\0')
          strcpy(content_type,local_default_type);
         else strcpy(content_type,reqInfo->hostInfo->default_type);
         return;
    }
    ++i;
    p=types[hash(fn[i])];

    while(p) {
        if(!strcasecmp(p->ext,&fn[i])) {
            strcpy(content_type,p->ct);
            return;
        }
        p=p->next;
    }
    if (local_default_type[0] != '\0')
      strcpy(content_type,local_default_type);
     else strcpy(content_type,reqInfo->hostInfo->default_type);
}


void probe_content_type(per_request *reqInfo, char *file) 
{
    find_ct(reqInfo,file,0);
}

void set_content_type(per_request *reqInfo, char *file) 
{
    find_ct(reqInfo,file,1);
}


/* Should remove all the added types from .htaccess files when the 
   child sticks around */

void reset_mime_vars(void) 
{
  struct mime_ext *mimes,*tmp;

  mimes = forced_types;
  tmp = mimes;
  while (mimes && (mimes != Saved_Forced)) {
    mimes = mimes->next;
    free(tmp->ext);
    free(tmp->ct);
    free(tmp);
    tmp = mimes;
  }

  forced_types = Saved_Forced;

  mimes = encoding_types;
  tmp = mimes;

  while (mimes && (mimes != Saved_Encoding)) {
    mimes = mimes->next;
    free(tmp->ext);
    free(tmp->ct);
    free(tmp);
    tmp = mimes;
  }

  encoding_types = Saved_Encoding;
}

void add_type(per_request *reqInfo, char *fn, char *t) {
    struct mime_ext *n;

    if(!(n=(struct mime_ext *)malloc(sizeof(struct mime_ext))))
        die(reqInfo,SC_NO_MEMORY,"add_type");

    if(!(n->ext = strdup(fn)))
        die(reqInfo,SC_NO_MEMORY,"add_type");
    if(!(n->ct = strdup(t)))
        die(reqInfo,SC_NO_MEMORY,"add_type");
    n->next = forced_types;
    forced_types = n;
}

void add_encoding(per_request *reqInfo, char *fn, char *t) {
    struct mime_ext *n;

    if(!(n=(struct mime_ext *)malloc(sizeof(struct mime_ext))))
        die(reqInfo, SC_NO_MEMORY,"add_encoding");

    if(!(n->ext = strdup(fn)))
        die(reqInfo, SC_NO_MEMORY,"add_encoding");
    if(!(n->ct = strdup(t)))
        die(reqInfo, SC_NO_MEMORY,"add_encoding");
    n->next = encoding_types;
    encoding_types = n;
}

void set_content_length(per_request *reqInfo, int l) {
    content_length = l;
}

int set_last_modified(per_request *reqInfo, time_t t) {
    struct tm *tms;
    char ts[MAX_STRING_LEN];

    tms = gmtime(&t);
    strftime(ts,MAX_STRING_LEN,HTTP_TIME_FORMAT,tms);
    strcpy(last_modified,ts);

    if(!ims[0])
        return 0;

    if(later_than(tms, ims))
        return die(reqInfo,SC_USE_LOCAL_COPY,NULL);

    return 0;
}

void init_header_vars(per_request *reqInfo) 
{
    content_type[0] = '\0';
    content_type_in[0] = '\0';
    last_modified[0] = '\0';
    content_length = -1;
    auth_line[0] = '\0';
    content_encoding[0] = '\0';
    location[0] = '\0';
    ims[0] = '\0';
    if (status_line != NULL) free(status_line);
    status_line = NULL;
    if (out_headers != NULL) free(out_headers);
    out_headers = NULL;

#ifdef HAVE_KERBEROS
    out_auth_header[0] = '\0';
#endif /* HAVE_KERBEROS */
}

/* Globals for speed because of size, these are for get_mime_headers */
static char field_type[HUGE_STRING_LEN];
static char unrec_hdr[HUGE_STRING_LEN];
static char unrec_hdr_val[HUGE_STRING_LEN];

void get_mime_headers(per_request *reqInfo) 
{
    char *field_val;

#ifdef DIGEST_AUTH
    client_accepts_digest = assume_digest_support;
#endif /* DIGEST_AUTH */
    while(getline(reqInfo->connection_socket,field_type,HUGE_STRING_LEN-1,0,
		  timeout) != -1) {
        if(!field_type[0]) 
            return;

        if(!(field_val = strchr(field_type,':')))
            continue;
        *field_val++ = '\0';
        while(isspace(*field_val)) ++field_val;

        if(!strcasecmp(field_type,"Content-type")) {
            strncpy(content_type_in,field_val,MAX_STRING_LEN);
	    content_type_in[MAX_STRING_LEN-1] = '\0';
            continue;
        }
        if(!strcasecmp(field_type,"Authorization")) {
            strncpy(auth_line,field_val,HUGE_STRING_LEN);
	    auth_line[HUGE_STRING_LEN-1]='\0';
            continue;
        }
	if(!strcasecmp(field_type,"Host")) {
	    strncpy(called_hostname, field_val, MAX_STRING_LEN);
	    called_hostname[MAX_STRING_LEN-1] = '\0';
	}
        if(!strcasecmp(field_type,"Extension")) {
	    if (!strcasecmp(field_val, "Notify-Domain-Restriction"))
		reqInfo->bNotifyDomainRestricted = 1;
#ifdef DIGEST_AUTH
	    else if (!strcasecmp(field_val, "Security/Digest"))
		client_accepts_digest = 1;
#endif /* DIGEST_AUTH */
        }
        if(!strcasecmp(field_type,"Content-length")) {
            sscanf(field_val,"%d",&content_length);
            continue;
        }
        if(!strcasecmp(field_type,"Connection")) {
	    if (!strcasecmp(field_val, "Keep-Alive") && 
		keep_alive.bAllowKeepAlive)
		keep_alive.bKeepAlive = 1;
	}
        if(!strcasecmp(field_type,"User-agent")) {
	    strncpy(reqInfo->agent, field_val, HUGE_STRING_LEN);
	    reqInfo->agent[HUGE_STRING_LEN-1] = '\0';
        }
        if(!strcasecmp(field_type,"Referer")) {
	    strncpy(reqInfo->referer, field_val, HUGE_STRING_LEN);
	    reqInfo->referer[HUGE_STRING_LEN-1] = '\0';
        }
        if(!strcasecmp(field_type,"If-modified-since")) {
            strcpy(ims,field_val);
	    ims[MAX_STRING_LEN-1] = '\0';
	}
        http2cgi(unrec_hdr, field_type);
	strcpy (unrec_hdr_val, field_val);
        if(reqInfo->env) {
	    if(!merge_header(reqInfo,unrec_hdr,field_val)) 
		make_env_str(reqInfo,unrec_hdr,field_val);
        } 
	else 
	    make_env_str(reqInfo,unrec_hdr,field_val); 
    }
}


void dump_default_header(per_request *reqInfo) 
{
    fprintf(reqInfo->out,"Date: %s%c",gm_timestr_822(time(NULL)),LF);
    fprintf(reqInfo->out,"Server: %s%c",SERVER_VERSION,LF);
    if (reqInfo->hostInfo->annotation_server[0])
	fprintf(reqInfo->out,"Annotations-cgi: %s%c",
		reqInfo->hostInfo->annotation_server,LF);

/* Not part of HTTP spec, removed. */
/*    fprintf(fd,"MIME-version: 1.0%c",LF); */
}


/* This needs to malloc in case a CGI script passes back its own
 * status_line, so we can free without problem.
 */
char* set_stat_line(per_request *reqInfo) 
{
    if (status_line) free(status_line);

    switch (reqInfo->status) {
    case 302:
	status_line = strdup((char *)StatLine302);
	break;
      case 304:
	status_line = strdup((char *)StatLine304);
	break;
    case 400:
	status_line = strdup((char *)StatLine400);
	break;
    case 401:
	status_line = strdup((char *)StatLine401);
	break;
    case 403:
	status_line = strdup((char *)StatLine403);
	break;
      case 404:
	status_line = strdup((char *)StatLine404);
	break;
    case 500:
	status_line = strdup((char *)StatLine500);
	break;
    case 501:
	status_line = strdup((char *)StatLine501);
	break;
    default:
	status_line = strdup((char *)StatLine200);
	break;
    }
    return status_line;
}

	
void send_http_header(per_request *reqInfo) 
{
    if(!status_line) {
        if(location[0]) {
            reqInfo->status = 302;
	    status_line = strdup((char *)StatLine302);
        }
        else {
	    set_stat_line(reqInfo);
        }
    }            
    begin_http_header(reqInfo,status_line);
    if(content_type[0]) 
        fprintf(reqInfo->out,"Content-type: %s%c",content_type,LF);
    if(last_modified[0])
        fprintf(reqInfo->out,"Last-modified: %s%c",last_modified,LF);
    if(content_length >= 0) 
        fprintf(reqInfo->out,"Content-length: %d%c",content_length,LF);
    if(location[0])
        fprintf(reqInfo->out,"Location: %s%c",location,LF);
    if(content_encoding[0])
        fprintf(reqInfo->out,"Content-encoding: %s%c",content_encoding,LF);

    if (reqInfo->bNotifyDomainRestricted && reqInfo->bSatisfiedDomain)
	fprintf(reqInfo->out,"Extension: Domain-Restricted%c", LF);

    keep_alive.bKeepAlive = keep_alive.bKeepAlive && (content_length >= 0);
    if (keep_alive.bKeepAlive && (!keep_alive.nMaxRequests ||
				  keep_alive.nCurrRequests + 1 < 
				  keep_alive.nMaxRequests)) {
	keep_alive.bKeepAlive = 1;
	fprintf(reqInfo->out,
		"Connection: Keep-Alive%cKeep-Alive: max=%d, timeout=%d%c",
		LF, keep_alive.nMaxRequests, keep_alive.nTimeOut, LF);
    }
    if(out_headers)
        fprintf(reqInfo->out,"%s",out_headers);
#ifdef HAVE_KERBEROS
    if (out_auth_header[0])
	fprintf (reqInfo->out, "%s", out_auth_header);
#endif /* HAVE_KERBEROS */
    fprintf(reqInfo->out,"%c",LF);
    fflush(reqInfo->out);  
}
