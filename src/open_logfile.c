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
 * open_logfile.c,v 1.6 1995/11/28 09:02:17 blong Exp
 *
 ************************************************************************
 *
 * open_logfile.c:  Save me from myself safe logs openning to prevent security
 *	 		 holes.
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
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>

#include "constants.h"
#include "http_config.h"
#include "open_logfile.h"
#include "util.h"


/* Enough systems don't have this that we define it here if necessary 
 */
#ifndef S_ISLNK 
#define S_ISLNK(m) (((m)&(S_IFMT)) == (S_IFLNK)) 
#endif /* S_ISLNK */ 


struct mystat
{
    struct stat real, link;
    int rok, lok;
};

#ifdef DEBUG 
/* This is used yet, so no reason to compile it in */
static int is_symbolic_link(struct mystat *x)
{
     return x->lok == 0 && (S_ISLNK(x->link.st_mode));
}
#endif /* DEBUG */

static int group_write_allowed(struct mystat *x)
{
    return x->rok == 0 && (S_IWGRP & x->real.st_mode);
}

static int other_write_allowed(struct mystat *x)
{
    return x->rok == 0 && (S_IWOTH & x->real.st_mode);
}

static int inode_changed (struct mystat *before, struct mystat *after)
{
    if (before->rok != 0) return 0;
    if (before->lok != 0) return 0;
    if (after->rok != 0) return 1;
    if (after->lok != 0) return 1;

    if (before->real.st_ino != after->real.st_ino) return 1;
    if (before->link.st_ino != after->link.st_ino) return 1;

    return 0;
}

static int running_as_root()
{
    return geteuid() == 0 ;
}

#ifdef DEBUG
/* this routine doesn't do what I need. It needs to return
 * a name which can be used "as is" - now it only returns
 * the name given on the ln -s command, which is a name relative
 * to the link name itself
 *  For example, if "logs/b"  is linked by
 *    cd logs
 *    ln -s b c
 *    ls -l
 *      lrwx------ c -> b
 *
 * then the link name returned for "logs/c" is "b" not "logs/b"
 * I need to work harder to get the name right
 */
static char * symbolic_link_name (char * name)
{
    static char linkname[1024];
    int n;

    n = readlink (name, linkname, sizeof(linkname) - 1);
    if (n <= 0) return (char *)NULL;
    linkname[n] = 0;
    return linkname;
}
#endif /* DEBUG */

static char * parent_directory (char *name)
{
    char *slash;
    char *dir;

    dir = strdup(name);
    slash = strrchr(dir,'/');
    if (slash)
    {
	*slash = 0;
    }
    else
    {
	free (dir);
	dir = strdup (".");
    }
    return dir;
}

static void mystat(char *name, struct mystat *x)
{
    x->rok = stat (name, &x->real);
    x->lok = lstat (name, &x->link);
}

static void check_before (char *name, struct mystat *before)
{
    char *dir;

    if (!running_as_root()) return;

    dir = parent_directory (name);
    mystat (dir, before);

    if (!log_directory_group_write_ok && group_write_allowed(before))
    {
	fprintf (stderr, "ERROR: log directory <%s> has 'group' write permission set\n", dir);
	exit(1);
    }
    if (!log_directory_other_write_ok && other_write_allowed(before))
    {
	fprintf (stderr, "ERROR: log directory <%s> has 'other' write permission set\n", dir);
	exit(1);
    }

    free (dir);
}

static void check_after (char *name, struct mystat *before, struct mystat *after)
{
    char *dir;
    if (!running_as_root()) return;

    dir = parent_directory (name);
    mystat (dir, after);

    if(inode_changed (before, after))
    {
	fprintf (stderr, "WARNING: possible security breach\n");
	fprintf (stderr, "Log directory <%s> has new inode number after opening logfile <%s>\n", dir, name);
	fprintf (stderr, "This means that the log directory <%s> has been moved or removed and a new one put in it's place after the log file <%s> had been opened\n", dir, name);
	exit(1);
    }

    free (dir);
}

FILE * fopen_logfile (char *name, char *mode)
{
    struct mystat before, after;
    FILE *fd;

    check_before (name, &before);

    fd = fopen (name, mode);
    if (fd == NULL) return fd;

    check_after (name, &before, &after);

    return fd;
}

int open_logfile (char *name, int flags, mode_t mode)
{
    struct mystat before, after;
    int fd;

    check_before (name, &before);

    fd = open (name, flags, mode);
    if (fd < 0) return fd;

    check_after (name, &before, &after);

    return fd;
}
