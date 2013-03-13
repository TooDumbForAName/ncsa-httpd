/************************************************************************
 * dbmgroup : a support program for NCSA HTTPd 1.5+ which handles
 *            dbm based group files.
 * WARNING: This program doesn't do any locking of the dbm files (a 
 *          nightmare for portability) and DBM can't handle multiple
 *          simultaneous accesses.
 ************************************************************************
 *
 * dbmgroup.c,v 1.8 1995/11/28 09:11:05 blong Exp
 *
 ************************************************************************
 *
 * 11-09-95 blong
 *     Written for NCSA HTTPd 1.5 release, initial version
 *
 */

#include "config.h"
#include "portability.h"

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <sys/signal.h>
#include <stdlib.h>
#include <time.h>
#include <ndbm.h>
#include <fcntl.h>
#include <sys/stat.h>


/* Currently defined commands
 */
#define DBM_ADDUSER   1
#define DBM_DELUSER   2
#define DBM_LIST      3
#define DBM_ADDGROUP  4
#define DBM_DELGROUP  5
#define DBM_LISTGROUP 6


void usage(char *name) {
  fprintf(stderr,"\nDBM Groupfile Editor for NCSA HTTPd 1.5\n\n");
  fprintf(stderr,"Usage: %s [-c] adduser   dbmgroupfile group user\n",name);
  fprintf(stderr,"       %s      deluser   dbmgroupfile group user\n",name);
  fprintf(stderr,"       %s      list      dbmgroupfile\n",name);
  fprintf(stderr,"       %s [-c] addgroup  dbmgroupfile group\n",name);    
  fprintf(stderr,"       %s      delgroup  dbmgroupfile group\n",name);    
  fprintf(stderr,"       %s      listgroup dbmgroupfile group\n",name);
  fprintf(stderr,"The -c flag creates a new file.\n");
  exit(1);
}

/*
 * Create a new dbmgroupfile.  If it exists, truncate it.
 */
DBM* create_dbm_file(char *filename) {
  DBM *db;
  mode_t mode;
  mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
  
  db = dbm_open(filename, O_RDWR | O_CREAT | O_TRUNC, mode);
  if (!db) {
    fprintf(stderr,"Can't create the database %s.\n", filename);
    perror("dbm_open");
    exit(1);
  }
  else fprintf(stderr,"Created the database %s.\n", filename);
  return db;
}


/*
 * Open a dbmgroupfile
 */
DBM *open_dbm_file(char *filename) {
  DBM *db;
  mode_t mode;

  mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
  db = dbm_open(filename, O_RDWR, mode);
  if (!db) {
    fprintf(stderr, "Could not open dbmgroup file %s.\n",filename);
    perror("dbm_open");
    fprintf(stderr, "Use -c option to create a new one.\n");
    exit(1);
  }
  return db;
}

void dbm_list(DBM *dbmgroup) {
  datum key,resp;

  for (key = dbm_firstkey(dbmgroup); key.dptr !=  NULL;  key = dbm_nextkey(dbmgroup)) {
    resp = dbm_fetch(dbmgroup,key);
    printf("Group: %s\n",key.dptr);
    if (resp.dptr) {
      printf(" Users: %s\n",resp.dptr);
    } else {
      printf(" Empty\n");
    }
  }
}

void dbm_list_group(DBM *dbmgroup,char *group) {
  datum key,resp;

  key.dptr = group;
  key.dsize = strlen(group);
  resp = dbm_fetch(dbmgroup,key);
  printf("Group: %s\n",key.dptr);
  if (resp.dptr) {
    printf(" Users: %s\n",resp.dptr);
  } else {
    printf(" Empty\n");
  }
}

void dbm_add_user(DBM *dbmgroup, char *group, char *user) {
  datum key,resp;
  char *newusers;
  char *tmp;

  key.dptr = group;
  key.dsize = strlen(group);
  resp = dbm_fetch(dbmgroup,key);
  if (resp.dptr) {
    printf("Adding %s to group %s.\n",user,group);
    if (resp.dptr[0]) {
      if (!(newusers = (char *)malloc(resp.dsize + strlen(user) + 2))) {
	fprintf(stderr,"Not enough memory to complete operation.\n");
	perror("malloc");
	exit(1);
      }
      tmp = resp.dptr;
      while ((tmp = strstr(tmp,user))) {
	if ((tmp[strlen(user)] == '\0') || (tmp[strlen(user)] == ' ') ||
	    (tmp[strlen(user)] == ',')) {
	  printf("User %s is already in group %s.\n",user,group);
	  exit(0);
	} else tmp++;
      }
      strcpy(newusers,resp.dptr);
      sprintf(newusers,"%s %s",newusers,user);
      resp.dptr = newusers;
      resp.dsize = strlen(newusers);
      dbm_store(dbmgroup,key,resp,DBM_REPLACE);
    } else {
      resp.dptr = user;
      resp.dsize = strlen(user);
      dbm_store(dbmgroup,key,resp,DBM_INSERT);
    }
  } else {
    printf("Creating group %s and adding user %s.\n",group,user);
    resp.dptr = user;
    resp.dsize = strlen(user);
    dbm_store(dbmgroup,key,resp,DBM_INSERT);
  }
}
    
void dbm_del_user(DBM *dbmgroup, char *group, char *user) {
  datum key, resp;
  char *userlist;
  char *tmp;

  key.dptr = group;
  key.dsize = strlen(group);
  resp = dbm_fetch(dbmgroup,key);

  if (resp.dptr) {
    if (resp.dptr[0] != '\0') {
      if (!strcmp(resp.dptr,user)) {
	resp.dptr[0] = '\0';
	resp.dsize = 1;
	dbm_store(dbmgroup,key,resp,DBM_REPLACE);
      } else {
	tmp = resp.dptr;
	while ((tmp = strstr(tmp,user))) {
	  if ((tmp[strlen(user)] == '\0') || (tmp[strlen(user)] == ' ') ||
	      (tmp[strlen(user)] == ',')) {
	    break;
	  } else tmp++;
	}
	if (!tmp) {
	  fprintf(stderr,"The user %s is not in group %s.\n",user,group);
	  exit(1);
	}
	printf("Deleting user %s from group %s.\n",user,group);
	userlist = (char *)malloc(strlen(resp.dptr) - strlen(user)+1);
	if (tmp != resp.dptr) {
	  strncpy(userlist,resp.dptr,tmp-resp.dptr-1);
	  strcat(userlist,(tmp+strlen(user)));
	} else {
	  strcpy(userlist,(tmp+strlen(user)));
	}
	resp.dptr = userlist;
	resp.dsize = strlen(userlist);
	dbm_store(dbmgroup,key,resp,DBM_REPLACE);
      }
    } else {
      fprintf(stderr,"The group %s is emptry.\n",group);
      exit(1);
    }
  } else {
    fprintf(stderr,"There is no group %s.\n",group);
    exit(1);
  }
}
    
void dbm_add_group(DBM *dbmgroup, char *group) {
  datum key,resp;
  char tmp[5];

  key.dptr = group;
  key.dsize = strlen(group);
  resp = dbm_fetch(dbmgroup,key);
  if (resp.dptr) {
    fprintf(stderr,"Group %s already exists.\n",group);
    exit(1);
  } else {
    printf("Adding group %s.\n",group);
    resp.dptr = tmp;
    tmp[0] = '\0';
    resp.dsize = 1;
    dbm_store(dbmgroup,key,resp,DBM_INSERT);
  }
}

void dbm_del_group(DBM *dbmgroup, char *group) {
  datum key,resp;
  
  key.dptr = group;
  key.dsize = strlen(group);

  resp = dbm_fetch(dbmgroup,key);
  if (resp.dptr) {
    printf("Deleting group %s.\n",group);
    dbm_delete(dbmgroup,key);
  } else {
    fprintf(stderr,"Group %s does not exist.\n",group);
    exit(1);
  }
}

main(int argc, char *argv[]) {
    DBM* dbmgroup;
    FILE* tn;
    char *user = NULL;
    char *group = NULL;
    char *filename = NULL;
    int command = 0;
    int create = 0;
    int x=0;


    /* Parse command line
     */
    if (argc < 3) usage(argv[0]);

    x = 1;
    if (!strcmp(argv[x],"-v")) usage(argv[0]);
    if (!strcmp(argv[x],"-c")) {
      create = 1;
      x++;
    }
    if (!strcmp(argv[x],"adduser")) {
      command = DBM_ADDUSER;
      if (argc != x+4) usage(argv[0]);
      filename = argv[x+1];
      group = argv[x+2];
      user = argv[x+3];
    } else if (!strcmp(argv[x],"deluser")) {
      if (create) usage(argv[0]);
      command = DBM_DELUSER;
      if (argc != x+4) usage(argv[0]);
      filename = argv[x+1];
      group = argv[x+2];
      user = argv[x+3];
    } else if (!strcmp(argv[x],"list")) {
      if (create) usage(argv[0]);
      command = DBM_LIST;
      if (argc != x+2) usage(argv[0]);
      filename = argv[x+1];
    } else if (!strcmp(argv[x],"addgroup")) {
      command = DBM_ADDGROUP;
      if (argc != x+3) usage(argv[0]);
      filename = argv[x+1];
      group = argv[x+2];
    } else if (!strcmp(argv[x],"delgroup")) {
      if (create) usage(argv[0]);
      command = DBM_DELGROUP;
      if (argc != x+3) usage(argv[0]);
      filename = argv[x+1];
      group = argv[x+2];
    } else if (!strcmp(argv[x],"listgroup")) {
      if (create) usage(argv[0]);
      command = DBM_LISTGROUP;
      if (argc != x+3) usage(argv[0]);
      filename = argv[x+1];
      group = argv[x+2];
    } else usage(argv[0]);

    /* Open/Create dbmgroupfile
     */
    if (create) dbmgroup = create_dbm_file(filename);
    else dbmgroup = open_dbm_file(filename);

    /* Do commands
     */
    switch(command) {
    case DBM_ADDUSER:
      dbm_add_user(dbmgroup,group,user);
      break;
    case DBM_DELUSER:
      dbm_del_user(dbmgroup,group,user);
      break;
    case DBM_LIST:
      dbm_list(dbmgroup);
      break;
    case DBM_ADDGROUP:
      dbm_add_group(dbmgroup,group);
      break;
    case DBM_DELGROUP:
      dbm_del_group(dbmgroup,group);
      break;
    case DBM_LISTGROUP:
      dbm_list_group(dbmgroup,group);
      break;
    }


}
