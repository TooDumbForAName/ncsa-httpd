/*
 * dbmpasswd.c: simple program for manipulating DBM password files
 * 
 * By NCSA httpd developement team
 * Yuxin Zhou 
 *
 * Introduced DBM and interactive interface so it's eaier for webmaster
 * to manage dbmpasswd database.
 * 
 */

#include "config.h"
#include "portability.h"

#include <sys/types.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/signal.h>
#include <stdlib.h>
#include <time.h>
#include <ndbm.h> 
#include <fcntl.h>

#define LF 10
#define CR 13
#define MAX_STRING_LENGTH 256


/*
 *  Get string input from the keyboard. 
 *  Do lenth check(MAX_STRING_LENGTH = 256).
 *  Truncate anything after that
 */
void input(char *str) {
  char in;
  int counter = 0;
  
  fflush(stdin);
  in = getchar();
  while(counter < MAX_STRING_LENGTH && in != '\n') {
    str[counter++] = in;
     in = getchar();
  }
  if (counter == MAX_STRING_LENGTH)
    str[counter - 1] = '\000';
  else str[counter] = '\000';
}

char *strd(char *s) {
  char *d;

  d=(char *)malloc(strlen(s) + 1);
  strcpy(d,s);
  return(d);
}

/* From local_passwd.c (C) Regents of Univ. of California blah blah */
static unsigned char itoa64[] =         /* 0 ... 63 => ascii - 64 */
"./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

to64(s, v, n)
register char *s;
register long v;
register int n;
{
  while (--n >= 0) {
    *s++ = itoa64[v&0x3f];
    v >>= 6;
  }
}


#ifdef HEAD_CRYPT
char *crypt(char *pw, char *salt); /* why aren't these prototyped in include */
#endif /* HEAD_CRYPT */

#ifdef HEAD_GETPASS
char *getpass(char *prompt);
#endif /* HEAD_GETPASS */

/*
 *Create a new database file.  If file exists,
 *trancate it and open a new one.
 */
DBM *create_file(char *filename) {
  DBM *db;
  mode_t mode;
  mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

  db = dbm_open(filename, O_RDWR | O_CREAT | O_TRUNC, mode);
  if (!db) 
    printf("Can't create the database %s.\n", filename);
  else printf("Created the database %s.\n", filename);
  return db;
}

/*
 *  Open an existing database.  If can't open, create a new one
 *  if the user wants.
 */
DBM *open_file(char *filename) {
  DBM *db;
  mode_t mode;
  char ask[MAX_STRING_LENGTH];

  mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
  db = dbm_open(filename, O_RDWR, mode);
  if (!db) {
    fprintf(stderr, "Can't open file %s.  Do you want to create it[y/n]?", 
	    filename);
    input(ask);
    if (ask[0] == 'y' || ask[0] == 'Y')
      db = create_file(filename);
    else {
      printf("Don't create.  Exit.\n");
      exit(0);
    }
  }
  return db;
}

/*Get passwd and return the cripted passwd*/
char *get_passwd()
{
  char *cpw;
  char *pw, salt[3];
  pw = strd(getpass("New password:")); 
  
  if(!strcmp(pw,getpass("Re-type new password:"))) 
    {
      (void)srand((int)time((time_t *)NULL));
      to64(&salt[0],rand(),2);
      cpw = crypt(pw,salt);
      free(pw);
    }
  else {
    printf("Passwords don't match.  Can't change.\n");
    return NULL;
  }
  return cpw;
}

/*
 *For interactive mode to display a menu
 *Return the valid choice the user entered.
 */
char select_menu() {
  int valid = 0;
  char input;
  while (!valid) {
    printf("\nYou may choose:\n");
    printf("A)dd a user;\n");
    printf("D)elete a user;\n");
    printf("M)odify a user's password;\n");
    printf("Q)uit;\n");
    printf("enter A, D, M or Q: ");
    fflush(stdin);
    input = getchar();
    switch(input) {
    case 'a': case 'A':
    case 'd': case 'D':
    case 'm': case 'M':
    case 'q': case 'Q': valid = 1; break;
    default: printf("%c is an invalid choice.\n", input);
    }
  }
  return input;
}

/*Prompt help message*/
void usage() {
  printf("Usage:\n\n");
  printf("dbmpasswd [database_name]\n");
  printf("             for interactive mode database manager.\n");
  printf("dbmpasswd -h\n");
  printf("             this help page;\n");
  printf("dbmpasswd database_name username\n");
  printf("             add a user to the database;\n");
  printf("dbmpasswd -[ a c d m ] database_name username\n");
  printf("         -a  add a user to the database;\n");
  printf("         -c  create database and add a user;\n");
  printf("         -d  delete a user from the database;\n");
  printf("         -m  modefy an exist user's password in the database.\n");
}

void interrupted() {
  fprintf(stderr,"Interrupted.\n");
  exit(1);
}

/*
 *Change user's passwd.  
 *Won't change if no such user in the database
 */
void modify(DBM *db, char *user) {
  char *cpw;
  datum passwd, passwd_str, user_str;
  int result;
  
  user_str.dptr = user;
  user_str.dsize = strlen(user);
  
  passwd = dbm_fetch(db, user_str);
  if (passwd.dsize) {
    cpw = get_passwd();
    if (cpw) {
      passwd_str.dptr = cpw;
      passwd_str.dsize = strlen(cpw);
      dbm_store(db, user_str, passwd_str, DBM_REPLACE);
      printf("User %s's password has been modified.\n", user);
    }
    else printf("Can't modify.\n"); 
  }
  else printf( "No user %s regestered. Can't modify.\n", user);
}

/*
 *Add a user and passwd to the database.
 *Won't add if the user is in the database.
 */
void add(DBM *db, char *user) {
  char *cpw;
  int result;
  datum passwd, user_str, passwd_str;
  user_str.dptr = user;
  user_str.dsize = strlen(user);

  passwd = dbm_fetch(db, user_str);
      if (!passwd.dsize) {
	cpw = get_passwd();
	if (cpw) {
	  passwd_str.dptr = cpw;
	  passwd_str.dsize = strlen(cpw);
	  dbm_store(db, user_str, passwd_str, DBM_INSERT);
	  printf("User %s has been added to the password database.\n", user);
	}
      }
      else printf("User %s exist in the database. Can't add.\n",user);
}

/*Delete the user if the user is in the database*/
void delete(DBM *db, char *user) {
  datum passwd;
  
  passwd.dptr = user;
  passwd.dsize = strlen(user);
  
  if (dbm_delete(db, passwd))
    fprintf(stderr, "Can't delele user %s!\n", user);
  else printf("User %s is deleted from the database\n", user);
}

/*
 *do_passwd is for command line arguments*/
void do_passwd(char *flag, char *filename, char *username) {
  DBM *db;
  mode_t mode, mode_flag;
  mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
  mode_flag = O_RDWR;
  
  if (flag[0] == '-' && filename[0] != '-') {
    if (flag[1] == 'c')
      db = create_file(filename);
    else if(flag[1] == 'a' || flag[1] == 'd' || flag[1] == 'm')
      db = open_file(filename);
  }
  else usage();

  if (db)
    switch (flag[1]) {
    case 'a': 
    case 'c': add(db, username); break;
    case 'm': modify(db, username); break;
    case 'd': delete(db, username); break;
    default: usage();
    }
}


void interactive(DBM *db) {
  char filename[MAX_STRING_LENGTH];
  char user[MAX_STRING_LENGTH];
  char choice;

  /*First check if there db is set. If not, ask for file name*/
  while(!db) {
    printf("Must open a password database before operation.\n");
    printf("Enter the password database name(\"exit\" to quit): ");
    input(filename);
    if(!strcasecmp(filename, "exit"))
      return;
    db = open_file(filename);
  }
  
  /*Get a choice from the menu*/
  choice = select_menu();
  
  /*loop for interactive mode*/
  while(choice != 'q' && choice != 'Q') {
    printf("Username: ");
    input(user);
    switch(choice) {
    case 'a': case 'A': add(db, user);break;
    case 'm': case 'M': modify(db, user);break;
    case 'd': case 'D': ;delete(db, user);break;
    }
    choice = select_menu();
  }
  dbm_close(db);
}

/*
 *If argc == 2, check argv[1] is -h.  If -h, go to help
 *Otherwise assume it's a filename and go to interactive mode.
 */
void inter_help(char *filename) {
  DBM *db = NULL;

  if (!strcasecmp(filename, "-h"))
    usage();
  else {
    db = open_file(filename);
    if (db)
      interactive(db);
  }
  return;
}

main(int argc, char *argv[]) {
  DBM  *db = NULL;
  datum passwd;
  
  signal(SIGINT,(void (*)())interrupted);
  
  switch(argc) {
  case 1: interactive(NULL); break;
  case 2: inter_help(argv[1]); break;
  case 3: do_passwd("-a", argv[1], argv[2]); break;
  case 4: do_passwd(argv[1], argv[2], argv[3]); break;
  default:usage();
  }

}









