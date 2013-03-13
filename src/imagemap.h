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
 * imagemap.h,v 1.3 1995/10/06 19:15:56 blong Exp
 *
 ************************************************************************
 *
 * imagemap.h:  This file contains all of the header information for the
 *    built in imagemap support for NCSA HTTPd
 *
 */

#ifndef _IMAGEMAP_H
#define _IMAGEMAP_H 1

#define MAXLINE 500
#define MAXVERTS 100
#define X 0
#define Y 1

void sendmesg(per_request* reqInfo, char *url, FILE* fp);
int pointinpoly(double point[2], double pgon[MAXVERTS][2]);
int pointincircle(double point[2], double coords[MAXVERTS][2]);
int pointinrect(double point[2], double coords[MAXVERTS][2]);

int isname(char);

int send_imagemap(per_request* reqInfo, struct stat* fi, char* path_args,
		  char allow_options);

#endif /* _IMAGE_MAP_H */
