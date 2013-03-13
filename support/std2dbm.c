/*
 * std2dbm.c: simple program to convert the traditional flat access files to
 *            the dbm access type files.
 * 
 *  6-8-95    Written by Stanford S. Guillory 
 */

#include "config.h"
#include "portability.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#ifndef NO_STDLIB_H
# include <stdlib.h>
#endif /* NO_STDLIB_H */
#include <ctype.h>
#include <ndbm.h>

#define MAX_STRING_LEN 1024
#define CR              13
#define LF              10


void usage(char* s) 
{
    fprintf(stderr,"Usage: %s flat-file dbm-file\n", s);
    exit(1);
}

int main (int argc, char *argv[]) 
{
    FILE *std_fp;
    DBM  *db;
    datum dtKey, dtRec;
    char line[MAX_STRING_LEN], user_list[MAX_STRING_LEN];
    char *tok;
    int  lineno = 1;
    mode_t  mode;

    if (argc != 3) 
	usage(argv[0]);

    if (!(std_fp = fopen(argv[1], "r"))) {
	fprintf(stderr,"%s: Could not open file %s for reading.\n",
		argv[0],argv[1]);
	perror("fopen");
	exit(1);
    }


    mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    if (!(db = dbm_open (argv[2], O_RDWR | O_CREAT | O_TRUNC, mode))) {
	fprintf(stderr,"%s: Could not create database file %s.\n", 
		argv[0],argv[1]);
        exit(1);
    }

    while (fgets (line, MAX_STRING_LEN, std_fp)) {
	char* ch;

	if (!(ch = strchr (line, ':'))) {
	    fprintf (stderr, "%s: Error in %s file on line %d\n", 
		     argv[0], argv[1], lineno);
	    exit (1);
	}
	*ch++ = '\0';
	dtKey.dptr = line;
	dtKey.dsize = strlen (dtKey.dptr);

	while (isspace (*ch))
	    ch++;

	tok = strtok (ch, " \n\t");
	strcpy (user_list, tok);
	while (tok = strtok (NULL, " \n\t")) {
	    strcat (user_list, " ");
	    strcat (user_list, tok);
	}	    

	dtRec.dptr = user_list;
	dtRec.dsize = strlen (user_list)+1;

	printf ("Storing data <%s>, length %d with key <%s>, length %d\n",
		dtRec.dptr, dtRec.dsize, dtKey.dptr, dtKey.dsize);
	if (dbm_store (db, dtKey, dtRec, DBM_INSERT) == 1)
	    fprintf (stderr, "%s: Duplicate key %s found on line %d of file %s\n",
		     argv[0], dtKey.dptr, lineno, argv[1]);
	lineno++;
    }
    dbm_close(db);
    exit(0);
}
