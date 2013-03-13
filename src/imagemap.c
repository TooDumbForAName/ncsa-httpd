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
 * $Id: imagemap.c,v 1.16 1996/04/05 21:14:15 blong Exp $
 *
 ************************************************************************
 *
 * imagemap.c:  Contains all of the code for the internal imagemap
 *
 */

#include "config.h"
#include "portability.h"

#include <stdio.h>
#include <string.h>
#ifndef NO_STDLIB_H
#include <stdlib.h>
#endif /* NO_STDLIB_H */
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include "constants.h"
#include "fdwrap.h"
#include "allocate.h"
#include "http_log.h"
#include "http_config.h"
#include "http_request.h"
#include "imagemap.h"
#include "cgi.h"

#ifdef IMAGEMAP_SUPPORT 

/* Error messages for imagemaps */

#define IMAP_ERR_INCORRECT_ARGS			1
#define IMAP_ERR_INCORRECT_COORDS		2
#define IMAP_ERR_CERN_MISSING_RIGHT_PAREN	3

char *imagemap_errors[] = {
"IMAP: Imagemap request requires two coordinates as arguments",
"IMAP: Imagemap args missing y value",
"IMAP: Imagemap args missing right paren"};


/* NCSA Imagemap files use: method URL coord1 coord2
 * CERN Imagemap files use: method (coord1) (coord2) URL
 * This version of imagemap will probably work with either in the same file,
 * as long as a full line is in one format or the other.
 */
int send_imagemap(per_request* reqInfo, struct stat* fi, char allow_options)
{
    char *input, *def, *szPoint, *url, *type;
    double testpoint[2], pointarray[MAXVERTS][2];
    int i, j, k;
    int error_num = 0;
    FILE *fp;
    char *t;
    double dist, mindist = -1;
    int sawpoint = 0;
    int sawparen = 0;
    int Found = 0;

    
    input = newString(HUGE_STRING_LEN,STR_TMP);
    def = newString(MAX_STRING_LEN,STR_TMP);
    szPoint = newString(MAX_STRING_LEN,STR_TMP);
    type = newString(MAX_STRING_LEN,STR_TMP);
    url = newString(MAX_STRING_LEN,STR_TMP);

    def[0] = '\0';
    strcpy(szPoint, reqInfo->args);

    if(!(t = strchr(szPoint,','))) {
        error_num = IMAP_ERR_INCORRECT_ARGS;
	goto imagemap_error;
    }

    *t++ = '\0';
    testpoint[X] = (double) atoi(szPoint);
    testpoint[Y] = (double) atoi(t);

    if(!(fp=FOpen(reqInfo->filename,"r"))){
	log_reason(reqInfo, "File permissions deny server access",
		   reqInfo->filename);
        freeString(input);
        freeString(def);
        freeString(szPoint);
        freeString(url);
        freeString(type);
	die(reqInfo, SC_FORBIDDEN, reqInfo->url);
    }

    while (!Found && fgets(input,HUGE_STRING_LEN,fp)) {
        char num[10];

        /* Skip lines with # as comments and blank lines */
        if((input[0] == '#') || (!input[0]))
            continue;

        type[0] = '\0';url[0] = '\0';

        /* Copy the shape keyword into type */
        for(i=0;!isspace(input[i]) && (input[i]);i++)
            type[i] = input[i];
        type[i] = '\0';

        /* Forward to next word */
        while(isspace(input[i])) ++i;

        /* If no coordinates, must be url for default, or NCSA format  */
	if (input[i] != '(') {
          for(j=0;input[i] && !isspace(input[i]);++i,++j)
            url[j] = input[i];
          url[j] = '\0';
        }

	/* Handle default keyword */
        if(!strcmp(type,"default") && !sawpoint) {
            strcpy(def,url);
            continue;
        }

        /* Looking for Coordinates */
        k=0;
        while (input[i]) {
	    /* Move over spaces and commas */
            while (isspace(input[i]) || input[i] == ',')
                i++;
	    
	    /* Under CERN, coordinates are in parenthesis */
            if (input[i] == '(') {
                sawparen = 1;
                while (isspace(input[++i]));
            }

            /* Copy digits into num array */
            j = 0;
            while (isdigit(input[i]))
                num[j++] = input[i++];

            num[j] = '\0';
            if (!j) break;
            pointarray[k][X] = (double) atoi(num);

            /* Skip to next digit */
            while (isspace(input[i]) || input[i] == ',')
                i++;
            /* Copy other number into num */
            j = 0;
            while (isdigit(input[i]))
                num[j++] = input[i++];
            num[j] = '\0';

            if (!j && !sawparen && k > 0) {
              pointarray[k++][Y] = -127;
              break;
            }
  
            if (j)
                pointarray[k++][Y] = (double) atoi(num);
            else {
		error_num = IMAP_ERR_INCORRECT_COORDS;
	        FClose(fp);
		goto imagemap_error;
            }
            
            /* End of parenthesis for coordinates under CERN */
            if (input[i] == ')') {
	      i++;
	      sawparen = 0;
	    } else if (sawparen) {
	      error_num = IMAP_ERR_CERN_MISSING_RIGHT_PAREN;
              FClose(fp);
              goto imagemap_error;	
	    }
        }
        if (url[0] == '\0' && input[i]) {
          while (isspace(input[i])) i++;
          for (j = 0; input[i] && !isspace(input[i]); ++i, ++j)
             url[j] = input[i];
          url[j] = '\0';
        }
        pointarray[k][X] = -1;
        if(!strncmp(type, "poly", 4))
            if(pointinpoly(testpoint,pointarray))
	       Found = 1;
        if(!strncmp(type, "circ", 4))
            if(pointincircle(testpoint,pointarray))
	      Found = 1;
        if(!strncmp(type, "rect", 4))
            if(pointinrect(testpoint,pointarray))
	      Found = 1;
        if(!strcmp(type,"point")) {
	    /* Don't need to take square root. */
	    dist = ((testpoint[X] - pointarray[0][X])
		    * (testpoint[X] - pointarray[0][X]))
		   + ((testpoint[Y] - pointarray[0][Y])
		      * (testpoint[Y] - pointarray[0][Y]));
	    /* If this is the first point, or the nearest, set the default. */
	    if ((! sawpoint) || (dist < mindist)) {
		mindist = dist;
	        strcpy(def,url);
	    }
	    sawpoint++;
	}
    }
    if(Found) {
      sendmesg(reqInfo, url, fp);
      goto imagemap_ok;
    } else {
      if(def[0]) {
        sendmesg(reqInfo, def, fp);
	goto imagemap_ok;
      }
    }
/* No reason to log each of these as an "error" */
/*    log_reason(reqInfo, "No default defined in imagemap.",
	       reqInfo->filename); */
    FClose(fp);
    freeString(input);
    freeString(def);
    freeString(szPoint);
    freeString(url);
    freeString(type);
    die(reqInfo, SC_NO_CONTENT, reqInfo->url);
    return 0;
 
 imagemap_ok:
    FClose(fp);
    freeString(input);
    freeString(def);
    freeString(szPoint);
    freeString(url);
    freeString(type);
    return 1;
 
 imagemap_error:
   freeString(input);
   freeString(def);
   freeString(szPoint);
   freeString(url);
   freeString(type);
   log_reason(reqInfo,imagemap_errors[error_num-1],reqInfo->filename);
   die(reqInfo,SC_BAD_IMAGEMAP,imagemap_errors[error_num-1]);
   return -1;
}

void sendmesg(per_request* reqInfo, char *url, FILE *fp)
{
    char *loc, *furl;

    loc = newString(HUGE_STRING_LEN,STR_REQ);
    furl = newString(HUGE_STRING_LEN,STR_REQ);

    FClose(fp);
    if (!strchr(url, ':')) {   /*** If not a full URL ***/
      if (url[0] != '/') { /*** Relative URL ***/ 
        char *last = strrchr(reqInfo->url,'/');
        int x = 0, y = 0;
        while (((reqInfo->url+x) <= last) && (y < HUGE_STRING_LEN)) {
          loc[y] = *(reqInfo->url+x);
          x++; y++;
        }
        loc[y] = '\0';
      } else {
        strncpy(loc,url,HUGE_STRING_LEN);
      }
      construct_url(furl, reqInfo->hostInfo, loc);
      die(reqInfo,SC_REDIRECT_TEMP,furl);
    } else {
      die(reqInfo,SC_REDIRECT_TEMP,url);
    }
}

int pointinrect(double point[2], double coords[MAXVERTS][2])
{
        return ((point[X] >= coords[0][X] && point[X] <= coords[1][X]) &&
        (point[Y] >= coords[0][Y] && point[Y] <= coords[1][Y]));
}

int pointincircle(double point[2], double coords[MAXVERTS][2])
{
        int radius1, radius2;

        /* If given a radius instead of a coordinate */
        if (coords[1][Y] == -127)
          radius1 = coords[1][X];
        else
          radius1 = ((coords[0][Y] - coords[1][Y]) * 
                     (coords[0][Y] - coords[1][Y])) + 
                    ((coords[0][X] - coords[1][X]) * 
                     (coords[0][X] - coords[1][X]));
        radius2 = ((coords[0][Y] - point[Y]) * (coords[0][Y] - point[Y])) +
                  ((coords[0][X] - point[X]) * (coords[0][X] - point[X]));
        return (radius2 <= radius1);
}

int pointinpoly(double point[2], double pgon[MAXVERTS][2])
{
        int i, numverts, inside_flag, xflag0;
        int crossings;
        double *p, *stop;
        double tx, ty, y;

        for (i = 0; pgon[i][X] != -1 && i < MAXVERTS; i++)
                ;
        numverts = i;
        crossings = 0;

        tx = point[X];
        ty = point[Y];
        y = pgon[numverts - 1][Y];

        p = (double *) pgon + 1;
        if ((y >= ty) != (*p >= ty)) {
                if ((xflag0 = (pgon[numverts - 1][X] >= tx)) ==
                (*(double *) pgon >= tx)) {
                        if (xflag0)
                                crossings++;
                }
                else {
                        crossings += (pgon[numverts - 1][X] - (y - ty) *
                        (*(double *) pgon - pgon[numverts - 1][X]) /
                        (*p - y)) >= tx;
                }
        }

        stop = pgon[numverts];

        for (y = *p, p += 2; p < stop; y = *p, p += 2) {
                if (y >= ty) {
                        while ((p < stop) && (*p >= ty))
                                p += 2;
                        if (p >= stop)
                                break;
                        if ((xflag0 = (*(p - 3) >= tx)) == (*(p - 1) >= tx)) {
                                if (xflag0)
                                        crossings++;
                        }
                        else {
                                crossings += (*(p - 3) - (*(p - 2) - ty) *
                                (*(p - 1) - *(p - 3)) / (*p - *(p - 2))) >= tx;
                        }
                }
                else {
                        while ((p < stop) && (*p < ty))
                                p += 2;
                        if (p >= stop)
                                break;
                        if ((xflag0 = (*(p - 3) >= tx)) == (*(p - 1) >= tx)) {
                                if (xflag0)
                                        crossings++;
                        }
                        else {
                                crossings += (*(p - 3) - (*(p - 2) - ty) *
                                (*(p - 1) - *(p - 3)) / (*p - *(p - 2))) >= tx;
                        }
                }
        }
        inside_flag = crossings & 0x01;
        return (inside_flag);
}

#endif /* IMAGEMAP_SUPPORT */
