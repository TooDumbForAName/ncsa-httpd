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
 * http_alias.c: Stuff for dealing with directory aliases
 * 
 * Based on NCSA HTTPd 1.3 by Rob McCool
 * 
 *  04-06-95 blong
 * 	Added Saved_ variables to allow reset of aliases to configured 
 *	only.  save_aliases is called from http_config, and 
 *	reset_to_saved_alias is called in the initialization of 
 *	transactions.
 *
 *  06-30-95 blong
 *	removed saved stuff, since we now don't have to add user directories
 *	to the aliases, and so they never change after startup
 *
 *  07-27-95 blong
 *	log access to unknown users directory as suggested by
 *	 Gioacchino La Vecchia (gio@di.unipi.it) 
 */


#include "config.h"
#include "portability.h"

#include <stdio.h>
#ifndef NO_STDLIB_H 
# include <stdlib.h>
#endif /* NO_STDLIB_H */
#include <string.h>
#include <pwd.h>
#include "constants.h"
#include "http_alias.h"
#include "http_config.h"
#include "http_log.h"
#include "util.h"

/*
static int Saved_num_alias = 0;
static int num_aliases = 0;
static alias aliases[MAX_ALIASES];
static int Saved_num_redirect = 0;
static int num_redirect = 0;
static alias redirect[MAX_ALIASES];
*/

/* To send stat() information to cgi.c */
int dirs_in_alias;

void free_aliases(lookup *trans) {
  int x;

  for (x=0; x < trans->num_aliases ; x++) {
    free(trans->aliases[x].fake);
    free(trans->aliases[x].real);
  }
  free(trans->aliases);
  free(trans);
}

void add_lookup(per_host *host, char *fake, char *real, int type) {
  lookup *translations = host->translations;
  int n;

  if (translations == NULL) {
    translations = (lookup *) malloc(sizeof(lookup));
    translations->aliases = (lookupRec *) malloc(TRANS_BEG_SIZE * 
						 sizeof(lookupRec));
    translations->num_aliases = 0;
    translations->max_aliases = TRANS_BEG_SIZE;
    host->translations = translations;
  }
  if (translations->num_aliases >= translations->max_aliases) {
    translations->aliases = (lookupRec *) realloc(translations->aliases,
						  ((translations->max_aliases+
						    TRANS_INC_SIZE) *
						   sizeof(lookupRec)));
    translations->max_aliases += TRANS_INC_SIZE;
  }
  n = translations->num_aliases;
  translations->aliases[n].fake = strdup(fake);
  translations->aliases[n].fake_len = strlen(fake);
  translations->aliases[n].real = strdup(real);
  translations->aliases[n].real_len = strlen(real);
  translations->aliases[n].type = type;
  translations->num_aliases++;
}
  

void add_alias(per_host *host, char *fake, char *real, int is_script) {
  char tmp[MAX_STRING_LEN];

  if (real[0] != '/') {
    make_full_path(((is_script == A_SCRIPT_CGI) ? server_root : 
		    host->document_root),
		   real,tmp);
    add_lookup(host,fake,tmp,is_script);
  } else 
    add_lookup(host,fake,real,is_script);
}

void add_redirect(per_host *host, char *fake, char *url,int type) {
  add_lookup(host,fake,url,type);
}

char fake[MAX_STRING_LEN+2],real[MAX_STRING_LEN],dname[HUGE_STRING_LEN];

int translate_name(per_request *reqInfo, char* url, char *filename) 
{
    register int x;
    char w[MAX_STRING_LEN];
    struct passwd *pw;
    
    getparents(url);
    
    if (reqInfo->hostInfo->translations != NULL) {
	lookup *trans = reqInfo->hostInfo->translations;

	for(x=0; x < trans->num_aliases ;x++) {
	    if(!strncmp(url,trans->aliases[x].fake,
			trans->aliases[x].fake_len)) {
		strncpy(filename,trans->aliases[x].real,HUGE_STRING_LEN);
		strncat(filename,url+trans->aliases[x].fake_len,
			HUGE_STRING_LEN - trans->aliases[x].real_len);
		return trans->aliases[x].type;
	    }
	}
    }

    if((reqInfo->hostInfo->user_dir[0]) && (url[0] == '/') 
       && (url[1] == '~')) {
	strcpy(dname,&url[2]);
	getword(w,dname,'/');
	if(!(pw=getpwnam(w))) {
	/* log missing user attempt as suggested by 
	   Gioacchino La Vecchia (gio@di.unipi.it)  */
	    log_reason(reqInfo,"user does not exist",reqInfo->url);
	    die(reqInfo,SC_NOT_FOUND,reqInfo->url);
        }
	fake[0] = '/';
	fake[1] = '~';
	strcpy(&fake[2],w);
	make_full_path(pw->pw_dir,reqInfo->hostInfo->user_dir,real);
	strcpy(filename,real);
	strcat(filename,url+strlen(w)+2);
	return A_STD_DOCUMENT;
    }
    /* no alias, add document root */
    strncpy(filename,reqInfo->hostInfo->document_root,HUGE_STRING_LEN);
    strncat(filename,url, HUGE_STRING_LEN - reqInfo->hostInfo->doc_root_len);
    return A_STD_DOCUMENT;
}
