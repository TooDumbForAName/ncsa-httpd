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
 * fdwrap.c,v 1.15 1996/04/05 18:54:46 blong Exp
 *
 ************************************************************************
 *
 * fdwrap.c	used to wrap file descriptors so that we can keep track
 * 		of open ones and close them when errors happen.  Should 
 *		make leaks next to impossible.
 *
 */

#include "config.h"
#include "portability.h"

#include <stdio.h>
#ifndef NO_STDLIB_H 
# include <stdlib.h>
#endif /* NO_STDLIB_H */
#ifdef DBM_SUPPORT
# ifndef _DBMSUPPORT_H  /* moronic OSs which don't protect their own include */
#  define _DBMSUPPORT_H  /* files from being multiply included */
#  include <ndbm.h>
# endif /* _DBMSUPPORT_H */
#endif /* DBM_SUPPORT */
#ifndef NO_MALLOC_H
# ifdef NEED_SYS_MALLOC_H
#  include <sys/malloc.h>
# else
#  include <malloc.h>
# endif /* NEED_SYS_MALLOC_H */
#endif /* NO_MALLOC_H */

#include "constants.h"
#include "fdwrap.h"
#include "host_config.h"
#include "http_log.h"

static FDTABLE* FdTab;
static int nSize;

void fd_error(char *err_msg)
{
    char S[MAX_STRING_LEN];
    sprintf(S,"fdwrap error: %s",err_msg);
    log_error(S,gConfiguration->error_log);
    exit(1);
}

void fd_warn(char *err_msg)
{
    char S[MAX_STRING_LEN];
    sprintf(S,"fdwrap warn: %s",err_msg);
    log_error(S,gConfiguration->error_log);
}

/* Can't call fd_error in Init since error_log isn't opened yet. */
void InitFdTable (void)
{
    int ndx;

    /* take care of failure here */
    FdTab = (FDTABLE*) malloc (INITIAL_TABSIZE * sizeof(FDTABLE));
    if (!FdTab) {
      fprintf(stderr,
        "HTTPd: Could not allocate memory for file descriptor tracking\n");
      perror("malloc");
      exit(1);
    }
    nSize = INITIAL_TABSIZE;

    for (ndx = 0; ndx < INITIAL_TABSIZE; ndx++) {
	FdTab[ndx].bOpen = FDW_CLOSED;
	FdTab[ndx].fp = NULL;
    }
}

int GrowTable (int fd)
{
  int ndx;

  /* take care of failure here */
  FdTab = (FDTABLE*) realloc ((char*)FdTab,
			      ((fd + 10) * sizeof(FDTABLE)));
  if (!FdTab) {
    fd_warn("GrowTable Failed");
    return 0;
  }

  for (ndx = nSize; ndx < fd + 10; ndx++) {
    FdTab[ndx].bOpen = FDW_CLOSED;
    FdTab[ndx].fp = NULL;
  }
  nSize = fd + 10;

  return 1;
}
    
FILE* FOpen (char* fname, char* mode)
{
    FILE* fp;
    int   fd;

    if ((fp = fopen (fname, mode))) {
	fd = fileno(fp);
	if (fd >= nSize) 
	  if (!GrowTable(fd)) {
	    fclose(fp);
	    return NULL;
          }

	FdTab[fd].bOpen = FDW_FILE_PTR;
	FdTab[fd].fp = fp;
	return fp;
    }
    else
	return NULL;
}

DIR* Opendir(char* dirname)
{
   DIR* dp;
   int fd;
   if ((dp = opendir(dirname))) {
       fd = DIR_FILENO(dp);
       if (fd >= nSize)
	 if (!GrowTable(fd)) {
	   closedir(dp);
	   return NULL;
        }
        FdTab[fd].bOpen = FDW_DIR_PTR;
        FdTab[fd].fp = dp;
        return dp;
    }
    else
        return NULL;
}

int Pipe (int* pd)
{
    if (pipe(pd) < 0)
	return -1;

    if (pd[0] >= nSize || pd[1] >= nSize) 
      if (!GrowTable(pd[0] > pd[1] ? pd[0] : pd[1])) {
	close(pd[0]);
	close(pd[1]);
	return -1;
      }
    FdTab[pd[0]].bOpen = FDW_FILE_DESC;
    FdTab[pd[0]].fp = NULL;
    FdTab[pd[1]].bOpen = FDW_FILE_DESC;
    FdTab[pd[1]].fp = NULL;

    return 0;
}
    
FILE* FdOpen(int fd, char* mode)
{
    FILE* fp;

    if ((fp = fdopen(fd, mode))) {
	FdTab[fd].bOpen = FDW_FILE_PTR;
	FdTab[fd].fp = fp;
	return fp;
    }
    else
	return NULL;
}


#ifdef DBM_SUPPORT
DBM* DBM_Open (char *dbm_name, int flags, int mode)
{
    DBM* dp;
    int fd;

    if ((dp = dbm_open (dbm_name, flags, mode))) {
	fd = dbm_dirfno(dp);  /* Hope this is portable across implementations */
	if (fd >= nSize)
	  if (!GrowTable(fd)) {
	    dbm_close(dp);
	    return NULL;
	  }
        FdTab[fd].bOpen = FDW_DBM_PTR;
	FdTab[fd].fp = dp;
	return dp;
    }
    else
	return NULL;
}
#endif /* DBM_SUPPORT */

int FClose (FILE* fp)
{
    int fd;
    
    if (fp != NULL) {
      fd = fileno(fp);

      FdTab[fd].bOpen = FDW_CLOSED;
      if (FdTab[fd].fp != fp) {
        fd_warn("Mismatched File Pointer (FILE), closing both");
        if (FdTab[fd].fp != NULL) fclose(FdTab[fd].fp);
      }
      FdTab[fd].fp = NULL;
      return fclose(fp);
    } else return -1;
}

int Closedir (DIR *dp) 
{
    int fd = DIR_FILENO(dp);

    FdTab[fd].bOpen = FDW_CLOSED;
    if (FdTab[fd].fp != dp) {
      fd_warn("Mismatched File Pointer (FILE), closing both");
      fclose(FdTab[fd].fp);
    }
    FdTab[fd].fp = NULL;
#ifndef APOLLO
    return closedir(dp);
#else
    closedir(dp);
    return;
#endif /* APOLLO */
}


#ifdef DBM_SUPPORT
void DBM_Close (DBM *db)
{
    int fd = dbm_dirfno(db);

    FdTab[fd].bOpen = FDW_CLOSED;
    if (FdTab[fd].fp != db) {
      fd_warn("Mismatched File Pointer (DBM), closing both");
      if (FdTab[fd].fp != NULL) dbm_close(FdTab[fd].fp);
    }
    FdTab[fd].fp = NULL;
    if (db != NULL) dbm_close(db);
}
#endif /* DBM_SUPPORT */

int Close(int fd)
{
    FdTab[fd].bOpen = FDW_CLOSED;
    return close(fd);
}    

int CloseAll(void)
{
    int ndx;
    int nAny = 0;

    for (ndx = 0; ndx < nSize; ndx++) {
	if (FdTab[ndx].bOpen) {
	    nAny = 1;
	    switch (FdTab[ndx].bOpen) {
	    case FDW_FILE_DESC: close(ndx);
				break;
	    case FDW_FILE_PTR: fclose(FdTab[ndx].fp);
			       break;
	    case FDW_DIR_PTR: closedir(FdTab[ndx].fp);
			      break;
#ifdef DBM_SUPPORT
	    case FDW_DBM_PTR: dbm_close(FdTab[ndx].fp);
			      break;
#endif /* DBM_SUPPORT */
	    }
	    FdTab[ndx].bOpen = FDW_CLOSED;
	    FdTab[ndx].fp = NULL;
	}
    }
    return nAny;
}

void DestroyFdTab(void)
{
  free (FdTab);
}
