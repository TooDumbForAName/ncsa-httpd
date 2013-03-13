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
 * blackout.h,v 1.1 1996/02/08 18:01:02 blong Exp
 *
 ************************************************************************
 *
 */

#ifndef _BLACKOUT_H
#define _BLACKOUT_H

/* function prototypes */
long send_fp_black(per_request *reqInfo, FILE *f, void (*onexit)(void));

#endif /* _BLACKOUT_H */

