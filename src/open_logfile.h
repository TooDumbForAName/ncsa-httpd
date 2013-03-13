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
 * open_logfile.h,v 1.2 1995/09/20 23:37:11 blong Exp
 *
 ************************************************************************
 *
 * open_logfile.h: Header file for safe log file openning
 *
 */

#ifndef _OPEN_LOGFILE_H
#define _OPEN_LOGFILE_H 1


FILE *fopen_logfile (char *name, char *mode);
int   open_logfile  (char *name, int flags, mode_t mode);

#endif /* _OPEN_LOGFILE_H */
