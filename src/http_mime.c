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
 * http_mime.c,v 1.106 1996/03/13 18:28:39 blong Exp
 *
 ************************************************************************
 *
 * http_mime.c: maintains the list of mime types, encodings.  Currently
 *  still contains functions for setting some HTTP headers, probably be
 *  moved eventually.
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
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include "constants.h"
#include "fdwrap.h"
#include "allocate.h"
#include "http_mime.h"
#include "http_log.h"
#include "http_config.h"
#include "http_access.h"
#include "env.h"
#include "http_request.h"
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
        fprintf(stderr,"HTTPd: could not open mime types file %s\n",
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

void find_ct(per_request *reqInfo, char *file, 
	     char *content_type, char *content_encoding) 
{
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
                if(content_encoding != NULL) {
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
    find_ct(reqInfo,file,reqInfo->outh_content_type,NULL);
}

void set_content_type(per_request *reqInfo, char *file) 
{
    find_ct(reqInfo,file,reqInfo->outh_content_type,
	    reqInfo->outh_content_encoding);
}

void get_content_type(per_request *reqInfo, char *file, 
		      char *content_type, char *content_encoding)
{
    find_ct(reqInfo,file,content_type,content_encoding);
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
    reqInfo->outh_content_length = l;
}

int set_last_modified(per_request *reqInfo, time_t t) {
    struct tm *tms;
/*    char ts[MAX_STRING_LEN]; */

    tms = gmtime(&t);
    strftime(reqInfo->outh_last_mod,MAX_STRING_LEN,HTTP_TIME_FORMAT,tms);
/*    strcpy(reqInfo->outh_last_mod,ts); */

    if(!reqInfo->inh_if_mod_since[0])
        return 0;

    if(later_than(tms, reqInfo->inh_if_mod_since))
        return die(reqInfo,SC_USE_LOCAL_COPY,NULL);

    return 0;
}


/* This needs to malloc in case a CGI script passes back its own
 * status_line, so we can free without problem.
 */
char* set_stat_line(per_request *reqInfo) 
{
    if (reqInfo->status_line) {
      freeString(reqInfo->status_line);
    }

    switch (reqInfo->status) {
    case 200:
	reqInfo->status_line = dupStringP((char *)StatLine200,STR_REQ);
	break;
    case 204:
	reqInfo->status_line = dupStringP((char *)StatLine204,STR_REQ);
	break;
    case 301:
	reqInfo->status_line = dupStringP((char *)StatLine301,STR_REQ);
	break;
    case 302:
	reqInfo->status_line = dupStringP((char *)StatLine302,STR_REQ);
	break;
    case 304:
	reqInfo->status_line = dupStringP((char *)StatLine304,STR_REQ);
	break;
    case 400:
	reqInfo->status_line = dupStringP((char *)StatLine400,STR_REQ);
	break;
    case 401:
	reqInfo->status_line = dupStringP((char *)StatLine401,STR_REQ);
	break;
    case 403:
	reqInfo->status_line = dupStringP((char *)StatLine403,STR_REQ);
	break;
    case 404:
	reqInfo->status_line = dupStringP((char *)StatLine404,STR_REQ);
	break;
    case 500:
	reqInfo->status_line = dupStringP((char *)StatLine500,STR_REQ);
	break;
    case 501:
	reqInfo->status_line = dupStringP((char *)StatLine501,STR_REQ);
	break;
    case 503:
	reqInfo->status_line = dupStringP((char *)StatLine503,STR_REQ);
	break;
    default:
	reqInfo->status_line = dupStringP((char *)StatLine200,STR_REQ);
	break;
    }
    return reqInfo->status_line;
}
