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
 * allocate.c,v 1.5 1996/04/05 18:54:28 blong Exp
 *
 ************************************************************************
 *
 * allocate.c:  Functions to allocate data types as needed
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

#include "constants.h"
#include "host_config.h"
#include "http_log.h"
#include "allocate.h"
#include "http_request.h"


string_list free_list;
string_list used_list;

int initialize_allocate()
{
   initialize_string_allocate();
   return 0;
}

int initialize_string_allocate()
{
  free_list.num = 0;
  free_list.first = NULL;

  used_list.num = 0;
  used_list.first = NULL;
  
  allocate_strings(&free_list,HUGE_STRING_LEN,5);
  allocate_strings(&free_list,MAX_STRING_LEN,5);
  return 0; 
}
  
int allocate_strings(string_list *slist, int length, int num)
{
  string_item *stmp;
  char *S;


  while (num) {
    if (!(S = (char *) malloc(length * sizeof(char))))
      return 1;

    S[0] = '\0';

    if (!(stmp = (string_item *) malloc(sizeof(string_item))))
      return 1;

    stmp->string = S;
    stmp->length = length;

    if (slist->num == 0) {
      slist->first = stmp;
      stmp->next = NULL;
      slist->num = 1;
    } else {
      stmp->next = slist->first;
      slist->first = stmp;
      slist->num++;
    }
    num--;
  }
  return 0;
}
      
int remove_string_item(string_list *slist, string_item *sitem) 
{
  string_item *stmp,*last;
  int x;

  stmp = slist->first;
  last = NULL;

  for(x = 0; x < slist->num ; x++) {
    if (stmp == sitem) {
      if (stmp == slist->first) {
	slist->first = slist->first->next;
      } else {
	last->next = stmp->next;
      }
      slist->num--;
      stmp->next = NULL;
      return TRUE;
    }
    last = stmp;
    stmp = stmp->next;
  }
  return FALSE;
}
 
int add_string_item(string_list *slist, string_item *sitem)
{
  if (slist->num == 0) {
    slist->first = sitem;
    slist->num = 1;
  } else {
    sitem->next = slist->first;
    slist->first = sitem;
    slist->num++;
  }
  return TRUE;
}


char* newString(int length, int type)
{
  int num, x;
  string_item *sitem;

  if (free_list.num == 0) {
    if ((length == HUGE_STRING_LEN) || (length == MAX_STRING_LEN))
      num = 5;
    else
      num = 1;
    allocate_strings(&free_list, length, num);
  }

  sitem = free_list.first;
  for(x = 0; x < free_list.num ; x++) {
    if (sitem->length == length) {
      remove_string_item(&free_list,sitem);
      add_string_item(&used_list,sitem);
      sitem->type = type;
      return sitem->string;
    }
    sitem = sitem->next;
  }
  
  /* Don't have a string of the correct size, make one.
   * Since allocate_strings adds strings to beginning of list, just return
   * first string in used_list.
   * In the future, might want to just return one which is larger.
   */

  allocate_strings(&used_list, length, 1);
  used_list.first->type = type;
  return used_list.first->string;
}

int freeString(char *string) 
{
  int x;
  string_item *sitem;

  if (string == NULL) return FALSE;

  sitem = used_list.first;
  for(x = 0; x < used_list.num ; x++) {
    if (sitem->string == string) {
      string[0] = '\0';
      remove_string_item(&used_list,sitem);
      add_string_item(&free_list,sitem);
      return TRUE;
    }
    sitem = sitem->next;
  }
  log_error("Attempt to Free String not in Use",gConfiguration->error_log);
  return FALSE;
}

/* Look up allocated size of string 
 * Success returns lenth of string
 * Failure returns -1
 */
int sizeofString(char *string)
{
  int x;
  string_item *sitem;

  if (string == NULL) return -1;

  sitem = used_list.first;
  for(x = 0; x < used_list.num ; x++) {
    if (sitem->string == string) {
      return sitem->length;
    }
    sitem = sitem->next;
  }
/*  log_error("String not allocated.",gConfiguration->error_log); */
  return -1;
}


int freeAllStrings(int type)
{
  
   string_item *sitem,*slast;

   slast = NULL;
   sitem = used_list.first;
   while (used_list.num && (sitem != NULL)) {
     if (sitem->type <= type) {
       sitem->string[0] = '\0';
       if (sitem == used_list.first) {
	 used_list.first = sitem->next;
	 used_list.num--;
	 sitem->next = NULL;
	 add_string_item(&free_list,sitem);
	 sitem = used_list.first;
       } else {
	 used_list.num--;
	 slast->next = sitem->next;
	 add_string_item(&free_list,sitem);
	 sitem = slast->next;
       }
     } else {
       slast = sitem;
       sitem = sitem->next;
     }
   }
   return TRUE;
}

/* dupStringP() : Promotes and copies strings to MAX/HUGE in order to 
 *   attempt to use already existing strings (no malloc).  This will
 *   truncate strings to HUGE length.
 */
char *dupStringP(char *str_in, int type)
{
   int req_len;
   char *str_out;

   req_len = strlen(str_in);
   if (req_len <= MAX_STRING_LEN) 
     req_len = MAX_STRING_LEN;
    else 
     req_len = HUGE_STRING_LEN;
    

    str_out = newString(req_len,type);
    strncpy(str_out,str_in,req_len);
    str_out[req_len-1] = '\0';

    return str_out;
}
