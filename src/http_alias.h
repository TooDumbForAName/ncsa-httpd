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
 * http_alias.h,v 1.9 1995/11/28 09:01:52 blong Exp
 *
 ************************************************************************
 *
 *
 *
 * http_alias.h contains:
 *	struct alias
 *	
 */


#ifndef _HTTP_ALIAS_H_
#define _HTTP_ALIAS_H_

/* globals defined in this module */

extern int dirs_in_alias;

/* structures defined in this module */

/* ------------------ aliases/redirects structures ------------------- */

#define TRANS_BEG_SIZE  20
#define TRANS_INC_SIZE  5

/* Include the string lengths instead of having to compute them on
   every connection */

typedef struct _lookupRec {
  char *fake;
  int fake_len;
  char *real;
  int real_len;
  int type;
} lookupRec;

typedef struct _lookup {
  lookupRec *aliases;
  int num_aliases;
  int max_aliases;
} lookup;

/* function prototypes */
void reset_aliases(void);
void dump_aliases(void);
void add_alias(per_host *host, char *f, char *r, int is_script);
void free_aliases(lookup *trans);
void add_redirect(per_host *host, char *f, char *url,int type);
int translate_name(per_request *reqInfo, char *url, char *name);
void unmunge_name(per_request *reqInfo, char *name);


#endif /* _HTTP_ALIAS_H_ */
