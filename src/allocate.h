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
 * allocate.h,v 1.2 1996/04/05 18:54:29 blong Exp
 *
 ************************************************************************
 *
 * allocate.h:  Functions to allocate data types as needed
 *
 */

#ifndef _ALLOCATE_H
#define _ALLOCATE_H 1

/* There are different levels of strings, depending on when they can
 * be "free'd".
 * STR_TMP can be free'd outside the function.
 * STR_REQ can be free'd at the end of the request.
 * STR_HUP can be free'd at Restart.
 */

#define STR_TMP		0
#define STR_REQ		2
#define STR_HUP		5

typedef struct _string_item {
  char *string;
  int length;
  int type;
  struct _string_item* next;
} string_item;

typedef struct _string_list {
  string_item* first;
  int num;
} string_list;

/* Public Interface */
int initialize_allocate(void);
char* newString(int length, int type);
char *dupStringP(char *str_in, int type);
int freeString(char *string); 
int freeAllStrings(int type);
int sizeofString(char *string);

/* Private Interface */
int initialize_string_allocate();
int allocate_strings(string_list *slist, int length, int num);
int remove_string_item(string_list *slist, string_item *sitem); 
int add_string_item(string_list *slist, string_item *sitem);

#endif /* _ALLOCATE_H */
