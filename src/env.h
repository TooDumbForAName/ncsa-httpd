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
 * env.h,v 1.8 1995/11/28 09:01:43 blong Exp
 *
 ************************************************************************
 *
 * env.h contains:
 *	All functions for dealing with the environment array.
 *	
 */

#ifndef _ENV_H_
#define _ENV_H_ 1

/* globals defined in this module */

/* Numbers to increase env array by
      These settings might affect performance quite a bit, should look into
      tuning them 
      */
#define ENV_BEG_SIZE 25
#define ENV_INC_SIZE 5
#define BIG_ENV_VAR_LEN 1024

/* function defined in this module */
int make_env_str(per_request *reqInfo, char *name, char *value);
int merge_header(per_request *reqInfo, char *h, char *v);
void free_env(per_request *reqInfo);
int replace_env_str(per_request *reqInfo, char *name, char *value);

#endif /* _ENV_H_ */

