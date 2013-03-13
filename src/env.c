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
 *env.c,v 1.17 1995/11/28 09:01:42 blong Exp
 *
 ************************************************************************
 *
 * env.c contains:
 *	All functions for dealing with the environment array.
 *	
 */

#include "config.h"
#include "portability.h"

#ifndef NO_STDLIB_H 
# include <stdlib.h>
#endif /* NO_STDLIB_H */
#include <string.h>
#include "constants.h"
#include "env.h"
#include "http_log.h"

/* Older version, required external help.  Newer version should be self 
   contained for easier extensibility */

/* This will change the value of an environment variable to *value
   if found.  Returns TRUE if the replace took place, FALSE otherwise */

int  replace_env_str(per_request *reqInfo, char *name, char *value)
{
    register int i, len;
 
    for (i = 0, len = strlen(name); reqInfo->env[i]; i++) {
	if (strncmp(reqInfo->env[i], name, len) == 0) {
	    free(reqInfo->env[i]);
	    if (i < reqInfo->num_env) {
		reqInfo->env[i] = reqInfo->env[--(reqInfo->num_env)];
		reqInfo->env_len[i] = reqInfo->env_len[reqInfo->num_env];
		reqInfo->env[reqInfo->num_env] = NULL;
	    } 
	    else {
		reqInfo->env[i] = NULL;
		reqInfo->num_env--;
	    }
	    make_env_str(reqInfo, name, value);
	    return TRUE;
	}
    }
    return FALSE;
}

/* Go down the array of environment variables, free'ing as you go */

void free_env(per_request *reqInfo) {
    int x;
    
    for(x=0;reqInfo->env[x];x++)
        free(reqInfo->env[x]);
    free(reqInfo->env);
    free(reqInfo->env_len);
    reqInfo->env = NULL;
}


/* If the environment variable has already been set, this will append
   the value to it, of the form "name=old, new" 
   Assumes that "header" is a pointer to a string that is longer than
   the string it contains
*/

int merge_header(per_request *reqInfo, char *header, char *value) 
{
    register int l,lt;
    int len, ndx;
    char **t;
    
    len = strlen(value);

    for(l=0;header[l];++l);
    header[l] = '=';
    header[++l] = '\0';

    for(ndx = 0, t=reqInfo->env; *t; ++t, ndx++) {
        if(!strncmp(*t,header,l)) {
            lt = strlen(*t);
	    if ((lt + len + 2) > reqInfo->env_len[ndx]) {
		int n = reqInfo->env_len[ndx] / BIG_ENV_VAR_LEN + 1;
		if(!(*t = (char *) realloc(*t,n * BIG_ENV_VAR_LEN*sizeof(char))))
		    die(reqInfo, SC_NO_MEMORY,"merge_header");
		reqInfo->env_len[ndx] = n * BIG_ENV_VAR_LEN;
	    }
            (*t)[lt++] = ',';
            (*t)[lt++] = ' ';
            strcpy(&((*t)[lt]),value);
            return 1;
        }
    }
    header[l-1] = '\0';
    return 0;
}

/* make_env_str will add the environment variable name=value to 
   the environment array of a per_request structure.  It will also
   auto grow the structure as necessary using ENV_BEG_SIZE and ENV_INC_SIZE */

int make_env_str(per_request *reqInfo, char *name, char *value) 
{
    int n;

    if (reqInfo->env == NULL) {
	if (!(reqInfo->env = (char **) malloc(ENV_BEG_SIZE * sizeof(char *)))
	    || !(reqInfo->env_len = (int*) malloc(ENV_BEG_SIZE * sizeof(int))))
	    die(reqInfo,SC_NO_MEMORY,"make_env_str:malloc");
	reqInfo->max_env = ENV_BEG_SIZE;
    }
    if ((reqInfo->num_env+1) >= reqInfo->max_env) {
	if (!(reqInfo->env = (char **) realloc(reqInfo->env, 
	       ((reqInfo->max_env+ENV_INC_SIZE) * sizeof(char *)))) 
	    || !(reqInfo->env_len = (int*) realloc(reqInfo->env_len, 
			(reqInfo->max_env + ENV_INC_SIZE) * sizeof(int))))
	    die(reqInfo,SC_NO_MEMORY,"make_env_str:realloc");
	reqInfo->max_env += ENV_INC_SIZE;
    }
    if (!(reqInfo->env[reqInfo->num_env] = 
	  (char *) malloc(n = (strlen(name) + strlen(value) + 2))))
	die(reqInfo,SC_NO_MEMORY,"make_env_str:add");
    strcpy(reqInfo->env[reqInfo->num_env], name);
    strcat(reqInfo->env[reqInfo->num_env],"=");
    strcat(reqInfo->env[reqInfo->num_env],value);
    reqInfo->env_len[reqInfo->num_env] = n;
  
    reqInfo->num_env++;
    reqInfo->env[reqInfo->num_env] = NULL; 

    return 1;
}

/* Debugging dump of environment array */
/*
void print_env (FILE* fp, char** env)
{
    int n = 0;
    while (env[n]) {
	fprintf (fp, "Var %d: %s\n", n, env[n]);
	n++;
    }
}
*/
