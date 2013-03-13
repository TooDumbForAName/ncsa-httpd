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
 * fdwrap.h,v 1.5 1995/11/28 09:01:45 blong Exp
 *
 ************************************************************************
 *
 * fdwrap.h contains all of the header information for the file descriptor
 * 	wrapper contained in fdwrap.c
 *
 */

#ifndef _FDWRAP_H
#define _FDWRAP_H	1

#ifdef DBM_SUPPORT
# ifndef _DBMSUPPORT_H  /* moronic OSs which don't protect their own include */
#  define _DBMSUPPORT_H  /* files from being multiply included */
#  include <ndbm.h>
# endif /* _DBMSUPPORT_H */
#endif /* DBM_SUPPORT */

#define INITIAL_TABSIZE 30
#define INCREMENT_TABSIZE 10

/* File descriptor types, for use in knowing how to close them */
#define FDW_CLOSED    0
#define FDW_FILE_DESC 1
#define FDW_FILE_PTR  2 
#define FDW_DIR_PTR   3
#define FDW_DBM_PTR   4

typedef struct {
    int bOpen;
    void* fp;
} FDTABLE;

void InitFdTable(void);
int GrowFdTable(int fd);
FILE* FOpen(char*, char*);
DIR* Opendir(char*);
FILE* FdOpen(int, char*);
int Pipe(int*);
#ifdef DBM_SUPPORT
DBM* DBM_Open (char* , int, int);
void DBM_Close (DBM*);
#endif /* DBM_SUPPORT */
int FClose(FILE*);
int Closedir(DIR*);
int Close(int);
int CloseAll(void); 

#endif /* _FDWRAP_H */
