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
 * http_dir.c: Handles the on-the-fly html index generation
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
#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>
#include "constants.h"
#include "http_request.h"
#include "http_dir.h"
#include "http_mime.h"
#include "http_log.h"
#include "http_config.h"
#include "http_send.h"
#include "http_alias.h"
#include "util.h"
#include "fdwrap.h"
#include "allocate.h"

/* Split each item list into two lists, the 0-th entry holds items which 
 * are valid over this request while the 1-st entry for those valids for
 * the life time of the server. -achille 
 */

static struct item *icon_list[2], *alt_list[2], *desc_list[2], *ign_list[2];
static struct item *hdr_list[2], *rdme_list[2], *opts_list[2];

static int dir_opts;

void init_indexing(int local) {
    icon_list[FI_LOCAL] = NULL;
    alt_list[FI_LOCAL] = NULL;
    desc_list[FI_LOCAL] = NULL;
    ign_list[FI_LOCAL] = NULL;
    hdr_list[FI_LOCAL] = NULL;
    rdme_list[FI_LOCAL] = NULL;
    opts_list[FI_LOCAL] = NULL;
    if (local == FI_GLOBAL) {
      icon_list[FI_GLOBAL] = NULL;
      alt_list[FI_GLOBAL] = NULL;
      desc_list[FI_GLOBAL] = NULL;
      ign_list[FI_GLOBAL] = NULL;
      hdr_list[FI_GLOBAL] = NULL;
      rdme_list[FI_GLOBAL] = NULL;
      opts_list[FI_GLOBAL] = NULL;
    }
}

void kill_item_list(struct item *p) {
    struct item *q;

    while(p) {
        if(p->apply_to) free(p->apply_to);
        if(p->apply_path) free(p->apply_path);
        if(p->data) free(p->data);
        q = p;
        p = p->next;
        free(q);
    }
}

void kill_indexing(int local) {

    kill_item_list(icon_list[FI_LOCAL]);
    kill_item_list(alt_list[FI_LOCAL]);
    kill_item_list(desc_list[FI_LOCAL]);
    kill_item_list(ign_list[FI_LOCAL]);
    kill_item_list(hdr_list[FI_LOCAL]);
    kill_item_list(rdme_list[FI_LOCAL]);
    kill_item_list(opts_list[FI_LOCAL]);
    
    if (local == FI_GLOBAL) {
      kill_item_list(icon_list[FI_GLOBAL]);
      kill_item_list(alt_list[FI_GLOBAL]);
      kill_item_list(desc_list[FI_GLOBAL]);
      kill_item_list(ign_list[FI_GLOBAL]);
      kill_item_list(hdr_list[FI_GLOBAL]);
      kill_item_list(rdme_list[FI_GLOBAL]);
      kill_item_list(opts_list[FI_GLOBAL]);
    }
    init_indexing(local);
}

struct item *new_item(per_request *reqInfo,int type, 
		      char *to, char *path, char *data) {
    struct item *p;

    if(!(p = (struct item *)malloc(sizeof(struct item))))
        die(reqInfo,SC_NO_MEMORY,"new_item");

    p->type = type;
    if(data) {
        if(!(p->data = strdup(data)))
            die(reqInfo,SC_NO_MEMORY,"new_item");
    } else
        p->data = NULL;

    if(to) {
        if(!(p->apply_to = (char *)malloc(strlen(to) + 2)))
            die(reqInfo,SC_NO_MEMORY,"new_item");
        if((type == BY_PATH) && (!is_matchexp(to))) {
            p->apply_to[0] = '*';
            strcpy(&p->apply_to[1],to);
        } else
            strcpy(p->apply_to,to);
    } else
        p->apply_to = NULL;

    if(!(p->apply_path = (char *)malloc(strlen(path) + 2)))
        die(reqInfo,SC_NO_MEMORY,"new_item");
    sprintf(p->apply_path,"%s*",path);

    return p;
}

void add_alt(per_request *reqInfo, int local, int type, 
	     char *alt, char *to, char *path) 
{
    struct item *p;

    if(type == BY_PATH) {
        if(!strcmp(to,"**DIRECTORY**"))
            strcpy(to,"^^DIRECTORY^^");
    }
    p = new_item(reqInfo,type,to,path,alt);
    p->next = alt_list[local];
    alt_list[local] = p;
}

void add_icon(per_request *reqInfo, int local, int type, 
	      char *icon, char *to, char *path) 
{
    struct item *p;
    char iconbak[MAX_STRING_LEN];

    strcpy(iconbak,icon);
    if(icon[0] == '(') {
        char alt[MAX_STRING_LEN];
        getword(alt,iconbak,',');
        add_alt(reqInfo,local,type,&alt[1],to,path);
        iconbak[strlen(iconbak) - 1] = '\0';
    }
    if(type == BY_PATH) {
        if(!strcmp(to,"**DIRECTORY**"))
            strcpy(to,"^^DIRECTORY^^");
    }
    p = new_item(reqInfo,type,to,path,iconbak);
    p->next = icon_list[local];
    icon_list[local] = p;
}

void add_desc(per_request *reqInfo, int local, int type, 
	      char *desc, char *to, char *path) 
{
    struct item *p;

    p = new_item(reqInfo,type,to,path,desc);
    p->next = desc_list[local];
    desc_list[local] = p;
}

void add_ignore(per_request *reqInfo, int local, char *ext, char *path) 
{
    struct item *p;

    p = new_item(reqInfo,0,ext,path,NULL);
    p->next = ign_list[local];
    ign_list[local] = p;
}

void add_header(per_request *reqInfo, int local, char *name, char *path) 
{
    struct item *p;

    p = new_item(reqInfo,0,NULL,path,name);
    p->next = hdr_list[local];
    hdr_list[local] = p;
}

void add_readme(per_request *reqInfo, int local, char *name, char *path) 
{
    struct item *p;

    p = new_item(reqInfo,0,NULL,path,name);
    p->next = rdme_list[local];
    rdme_list[local] = p;
}


void add_opts_int(per_request *reqInfo, int local, int opts, char *path) 
{
    struct item *p;

    p = new_item(reqInfo,0,NULL,path,NULL);
    p->type = opts;
    p->next = opts_list[local];
    opts_list[local] = p;
}

void add_opts(per_request *reqInfo, int local, char *optstr, char *path)
{
    char w[MAX_STRING_LEN];
    int opts = 0;

    while(optstr[0]) {
        cfg_getword(w,optstr);
        if(!strcasecmp(w,"FancyIndexing"))
            opts |= FANCY_INDEXING;
        else if(!strcasecmp(w,"IconsAreLinks"))
            opts |= ICONS_ARE_LINKS;
        else if(!strcasecmp(w,"ScanHTMLTitles"))
            opts |= SCAN_HTML_TITLES;
        else if(!strcasecmp(w,"SuppressLastModified"))
            opts |= SUPPRESS_LAST_MOD;
        else if(!strcasecmp(w,"SuppressSize"))
            opts |= SUPPRESS_SIZE;
        else if(!strcasecmp(w,"SuppressDescription"))
            opts |= SUPPRESS_DESC;
        else if(!strcasecmp(w,"None"))
            opts = 0;
    }
    add_opts_int(reqInfo,local,opts,path);
}


char *find_item(per_request *reqInfo, struct item *list[2], char *path, 
		int path_only) 
{
    struct item *p = NULL;
    int i;
    char *file_type,*file_encoding;

    file_type = newString(MAX_STRING_LEN,STR_TMP);
    file_encoding = newString(MAX_STRING_LEN,STR_TMP);
    
    for (i=0; i < 2; i++) {
      for (p = list[i]; p; p = p->next) {
        /* Special cased for ^^DIRECTORY^^ and ^^BLANKICON^^ */
        if((path[0] == '^') || (!strcmp_match(path,p->apply_path))) {
	  if(!(p->apply_to)) 
	    goto found_item;
	  else if(p->type == BY_PATH) {
	    if(!strcmp_match(path,p->apply_to))
	      goto found_item;
	  } else if(!path_only) {
	    char pathbak[MAX_STRING_LEN];
	    
	    strcpy(pathbak,path);
	    get_content_type(reqInfo,pathbak,file_type,file_encoding);
	    if(!file_encoding[0]) {
	      if(p->type == BY_TYPE) {
		if(!strcmp_match(file_type,p->apply_to)) 
		  goto found_item;
	      }
	    } else {
	      if(p->type == BY_ENCODING) {
		if(!strcmp_match(file_encoding,p->apply_to)) 
		  goto found_item;
	      }
	    }
	  }
        }
      }
    }
    freeString(file_type);
    freeString(file_encoding);
    return NULL;

  found_item:
    freeString(file_type);
    freeString(file_encoding);
    return p->data;
}

#define find_icon(r,p,t) find_item(r,icon_list,p,t)
#define find_alt(r,p,t) find_item(r,alt_list,p,t)
#define find_desc(r,p) find_item(r,desc_list,p,0)
#define find_header(r,p) find_item(r,hdr_list,p,0)
#define find_readme(r,p) find_item(r,rdme_list,p,0)


int ignore_entry(char *path) 
{
  int i;
  struct item *p = NULL;

  for (i=0; i < 2; i++)
    for (p = ign_list[i]; p; p = p->next)
      if(!strcmp_match(path,p->apply_path))
	if(!strcmp_match(path,p->apply_to))
	  return 1;
  return 0;
}

int find_opts(char *path) 
{
  int i;
  struct item *p = NULL;

  for (i=0; i < 2; i++)
    for (p = opts_list[i]; p; p = p->next)
      if(!strcmp_match(path,p->apply_path))
	return p->type;
  return 0;
}

int insert_readme(per_request *reqInfo, char *name, 
		  char *readme_fname, int rule) {
    char fn[MAX_STRING_LEN];
    FILE *r;
    struct stat finfo;
    int plaintext=0;

    make_full_path(name,readme_fname,fn);
    strcat(fn,".html");
    if(stat(fn,&finfo) == -1) {
        fn[strlen(fn)-5] = '\0';
        if(stat(fn,&finfo) == -1)
            return 0;
        plaintext=1;
        if(rule) rprintf(reqInfo,"<HR>%c",LF);
        rprintf(reqInfo,"<PRE>%c",LF);
    }
    else if(rule) rprintf(reqInfo,"<HR>%c",LF);
    if(!(r = FOpen(fn,"r")))
        return 0;
    send_fp(reqInfo,r,NULL);
    FClose(r);
    if(plaintext) rprintf(reqInfo,"</PRE>%c",LF);
    return 1;
}


char *find_title(per_request *reqInfo, char *filename) {
    char titlebuf[MAX_STRING_LEN], *find = "<TITLE>";
    char filebak[MAX_STRING_LEN];
    FILE *thefile;
    int x,y,n,p;
    char *file_type, *file_encoding;

    file_type = newString(MAX_STRING_LEN,STR_TMP);
    file_encoding = newString(MAX_STRING_LEN,STR_TMP);

    strcpy(filebak,filename);
    get_content_type(reqInfo,filebak,file_type,file_encoding);
    if(((!strcmp(file_type,"text/html")) ||
       (strcmp(file_type, INCLUDES_MAGIC_TYPE) == 0))
       && (!file_encoding[0])) {
        if(!(thefile = FOpen(filename,"r")))
	   goto not_found;
        n = fread(titlebuf,sizeof(char),MAX_STRING_LEN - 1,thefile);
        titlebuf[n] = '\0';
        for(x=0,p=0;titlebuf[x];x++) {
            if(toupper(titlebuf[x]) == find[p]) {
                if(!find[++p]) {
                    if((p = ind(&titlebuf[++x],'<')) != -1)
                        titlebuf[x+p] = '\0';
                    /* Scan for line breaks for Tanmoy's secretary */
                    for(y=x;titlebuf[y];y++)
                        if((titlebuf[y] == CR) || (titlebuf[y] == LF))
                            titlebuf[y] = ' ';
		    FClose(thefile);
		    freeString(file_type);
		    freeString(file_encoding);
                    return strdup(&titlebuf[x]);
                }
            } else p=0;
        }
	FClose(thefile);
    }
  not_found:
    freeString(file_type);
    freeString(file_encoding);
    return NULL;
}


void escape_html(char *fn) {
    register int x,y;
    char copy[MAX_STRING_LEN];

    strcpy(copy,fn);
    for(x=0,y=0;copy[y];x++,y++) {
        if(copy[y] == '<') {
            strcpy(&fn[x],"&lt;");
            x+=3;
        }
        else if(copy[y] == '>') {
            strcpy(&fn[x],"&gt;");
            x+=3;
        }
        else if(copy[y] == '&') {
            strcpy(&fn[x],"&amp;");
            x+=4;
        }
        else
            fn[x] = copy[y];
    }
    fn[x] = '\0';
}

struct ent *make_dir_entry(per_request *reqInfo, char *path, 
			   char *name) {
    struct ent *p;
    struct stat finfo;
    char t[MAX_STRING_LEN];
    char *tmp;

    if((name[0] == '.') && (!name[1]))
        return(NULL);

    make_full_path(path,name,t);

    if(ignore_entry(t))
        return(NULL);

    if(!(p=(struct ent *)malloc(sizeof(struct ent))))
        die(reqInfo,SC_NO_MEMORY,"make_dir_entry");
    if(!(p->name=(char *)malloc(strlen(name) + 2)))
        die(reqInfo,SC_NO_MEMORY,"make_dir_entry");

    if(dir_opts & FANCY_INDEXING) {
        if((!(dir_opts & FANCY_INDEXING)) || stat(t,&finfo) == -1) {
            strcpy(p->name,name);
            p->size = -1;
            p->icon = NULL;
            p->alt = NULL;
            p->desc = NULL;
            p->lm = -1;
        }
        else {
            p->lm = finfo.st_mtime;
            p->size = -1;
            p->icon = NULL;
            p->alt = NULL;
            p->desc = NULL;
            if(S_ISDIR(finfo.st_mode)) {
                if(!(p->icon = find_icon(reqInfo,t,1)))
		    if (p->icon != NULL) free(p->icon);
                    p->icon = find_icon(reqInfo,"^^DIRECTORY^^",1);
                if(!(tmp = find_alt(reqInfo,t,1))){
                    p->alt = (char *) malloc(sizeof(char)*4);
                    strcpy(p->alt,"DIR");
		}
		else {
		    p->alt = strdup(tmp);
		}
                p->size = -1;
                strncpy_dir(p->name,name, MAX_STRING_LEN);
            }
            else {
                p->icon = find_icon(reqInfo,t,0);
		tmp = find_alt(reqInfo,t,0);
		if (tmp != NULL) p->alt = strdup(tmp);
                p->size = finfo.st_size;
                strcpy(p->name,name);
            }
        }
        if((p->desc = find_desc(reqInfo,t)))
            p->desc = strdup(p->desc);
        if((!p->desc) && (dir_opts & SCAN_HTML_TITLES))
            p->desc = find_title(reqInfo,t);
    }
    else {
        p->icon = NULL;
        p->alt = NULL;
        p->desc = NULL;
        p->size = -1;
        p->lm = -1;
        strcpy(p->name,name);
    }
    return(p);
}


void send_size(per_request *reqInfo, size_t size) {

    if(size == -1) {
        rputs("    -",reqInfo);
    }
    else {
        if(!size) {
            rputs("   0K",reqInfo);
        }
        else if(size < 1024) {
            rputs("   1K",reqInfo);
        }
        else if(size < 1048576)
            rprintf(reqInfo,"%4dK",size / 1024);
        else
            rprintf(reqInfo,"%4dM",size / 1048576);
    }
}

void terminate_description(char *desc) {
    int maxsize = 23;
    register int x;
    
    if(dir_opts & SUPPRESS_LAST_MOD) maxsize += 17;
    if(dir_opts & SUPPRESS_SIZE) maxsize += 7;

    for(x=0;desc[x] && maxsize;x++) {
        if(desc[x] == '<') {
            while(desc[x] != '>') {
                if(!desc[x]) {
                    maxsize = 0;
                    break;
                }
                ++x;
            }
        }
        else --maxsize;
    }
    if(!maxsize) {
        desc[x] = '>';
        desc[x+1] = '\0';
    }

}

void output_directories(per_request *reqInfo, struct ent **ar,int n,char *name)
{
    int x;
    char anchor[2 * MAX_STRING_LEN + 64],t[MAX_STRING_LEN],t2[MAX_STRING_LEN];
    char *tp;

    if(name[0] == '\0') { 
        name[0] = '/'; name[1] = '\0'; 
    }
    /* aaaaargh Solaris sucks. */
    rflush(reqInfo);

    if(dir_opts & FANCY_INDEXING) {
        rputs("<PRE>",reqInfo);
        if((tp = find_icon(reqInfo,"^^BLANKICON^^",1)))
            rprintf(reqInfo,"<IMG SRC=\"%s\" ALT=\"     \"> ",tp);
        rprintf(reqInfo,"Name                   ");
        if(!(dir_opts & SUPPRESS_LAST_MOD))
            rprintf(reqInfo,"Last modified     ");
        if(!(dir_opts & SUPPRESS_SIZE))
            rprintf(reqInfo,"Size  ");
        if(!(dir_opts & SUPPRESS_DESC))
            rprintf(reqInfo,"Description");
        rprintf(reqInfo,"%c<HR>%c",LF,LF);
    }
    else {
        rputs("<UL>",reqInfo);
    }
        
    for(x=0;x<n;x++) {
        if((!strcmp(ar[x]->name,"../")) || (!strcmp(ar[x]->name,".."))) {
	    int i;

/* Fixes trailing slash for fancy indexing.  Thanks Roy ? */
            make_full_path(name,"../",t);
            getparents(t);
            if(t[0] == '\0') {
                t[0] = '/'; t[1] = '\0';
            }
	    i = strlen(t);
	    if(t[i-1] != '/') {
	      t[i-1] = '/'; t[i] = '\0';
            }
            escape_uri(t);
            sprintf(anchor,"<A HREF=\"%s\">",t);
            strcpy(t2,"Parent Directory</A>");
        }
        else {
            lim_strcpy(t,ar[x]->name, MAX_STRING_LEN);
            strcpy(t2,t);
            if(strlen(t2) > 21) {
                t2[21] = '>';
                t2[22] = '\0';
            }
            /* screws up formatting, but some need it. should fix. */
/*            escape_html(t2); */
            strcat(t2,"</A>");
            escape_uri(t);
            sprintf(anchor,"<A NAME=\"%s\" HREF=\"%s\">",t,t);
        }
        escape_url(t);

        if(dir_opts & FANCY_INDEXING) {
            if(dir_opts & ICONS_ARE_LINKS)
                rprintf(reqInfo,"%s",anchor);
            if((ar[x]->icon) || reqInfo->hostInfo->default_icon[0] 
		|| local_default_icon[0]) {
                rprintf(reqInfo,"<IMG SRC=\"%s\" ALT=\"[%s]\">",
                                ar[x]->icon ? ar[x]->icon :
                                local_default_icon[0] ?
                                local_default_icon : 
				reqInfo->hostInfo->default_icon,
                                ar[x]->alt ? ar[x]->alt : "   ");
            }
            if(dir_opts & ICONS_ARE_LINKS) {
                rputs("</A>",reqInfo);
            }
            rprintf(reqInfo," %s",anchor);
            rprintf(reqInfo,"%-27.27s",t2);
            if(!(dir_opts & SUPPRESS_LAST_MOD)) {
                if(ar[x]->lm != -1) {
                    struct tm *ts = localtime(&ar[x]->lm);
                    strftime(t,MAX_STRING_LEN,"%d-%b-%y %H:%M  ",ts);
                    rputs(t,reqInfo);
                }
                else {
                    rputs("                 ",reqInfo);
                }
            }
            if(!(dir_opts & SUPPRESS_SIZE)) {
                send_size(reqInfo,ar[x]->size);
                rputs("  ",reqInfo);
            }
            if(!(dir_opts & SUPPRESS_DESC)) {
                if(ar[x]->desc) {
                    terminate_description(ar[x]->desc);
                    rprintf(reqInfo,"%s",ar[x]->desc);
                }
            }
        }
        else
            rprintf(reqInfo,"<LI> %s %s",anchor,t2);
        rputc(LF,reqInfo);
    }
    if(dir_opts & FANCY_INDEXING) {
        rputs("</PRE>",reqInfo);
    }
    else {
        rputs("</UL>",reqInfo);
    }
}


int dsortf(struct ent **s1,struct ent **s2)
{
    return(strcmp((*s1)->name,(*s2)->name));
}

    
void index_directory(per_request *reqInfo)
{
    DIR *d;
    struct DIR_TYPE *dstruct;
    int num_ent=0,x;
    struct ent *head,*p,*q;
    struct ent **ar;
    char *tmp;


    if(!(d=Opendir(reqInfo->filename)))
        die(reqInfo,SC_FORBIDDEN,reqInfo->url);

    strcpy(reqInfo->outh_content_type,"text/html");
    if(reqInfo->http_version != P_HTTP_0_9) 
        send_http_header(reqInfo);

    if(reqInfo->method == M_HEAD) {
	Closedir(d);
        log_transaction(reqInfo);
	return;
    }

    reqInfo->bytes_sent = 0;

    dir_opts = find_opts(reqInfo->filename);

/* Spew HTML preamble */
    rprintf(reqInfo,"<HEAD><TITLE>Index of %s</TITLE></HEAD><BODY>",
                          reqInfo->url);
    rputc(LF,reqInfo);

    if((!(tmp = find_header(reqInfo, reqInfo->filename))) || 
	(!(insert_readme(reqInfo,reqInfo->filename,tmp,0))))
        rprintf(reqInfo, "<H1>Index of %s</H1>%c",reqInfo->url,LF);

/* 
 * Since we don't know how many dir. entries there are, put them into a 
 * linked list and then arrayificate them so qsort can use them. 
 */
    head=NULL;
    while((dstruct=readdir(d))) {
        if((p = make_dir_entry(reqInfo,reqInfo->filename,dstruct->d_name))) {
            p->next = head;
            head = p;
            num_ent++;
        }
    }
    if(!(ar=(struct ent **) malloc(num_ent*sizeof(struct ent *)))) {
	Closedir(d);
        die(reqInfo,SC_NO_MEMORY,"index_directory");
    }
    p=head;
    x=0;
    while(p) {
        ar[x++]=p;
        p = p->next;
    }
    
    qsort((void *)ar,num_ent,sizeof(struct ent *),
#ifdef ULTRIX_BRAIN_DEATH
          (int (*))dsortf);
#else
          (int (*)(const void *,const void *))dsortf);
#endif /* ULTRIX_BRAIN_DEATH */
     output_directories(reqInfo,ar,num_ent,reqInfo->url);
     free(ar);
     q = head;
     while(q) {
         p=q->next;
         free(q->name);
         if(q->desc)
             free(q->desc);
	 if(q->alt)
	     free(q->alt);
         free(q);
         q=p;
     }
     Closedir(d);
     if(dir_opts & FANCY_INDEXING)
         if((tmp = find_readme(reqInfo,reqInfo->filename)))
             insert_readme(reqInfo,reqInfo->filename,tmp,1);
     else {
         rputs("</UL>",reqInfo);
     }

     rputs("</BODY>", reqInfo);
     rflush(reqInfo);
     log_transaction(reqInfo);
}
