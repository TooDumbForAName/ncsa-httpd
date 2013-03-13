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
 * imagemap.c,v 1.7 1995/11/28 09:02:16 blong Exp
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
#include "http_log.h"
#include "fdwrap.h"
#include "imagemap.h"

#ifdef IMAGEMAP_SUPPORT 

extern int port;

int send_imagemap(per_request* reqInfo, struct stat* fi, char* path_args,
		  char allow_options)
{
    char input[MAX_STRING_LEN], def[MAX_STRING_LEN];
    char szPoint[MAX_STRING_LEN];
    double testpoint[2], pointarray[MAXVERTS][2];
    int i, j, k;
    FILE *fp;
    char *t;
    double dist, mindist = -1;
    int sawpoint = 0;
    
    def[0] = '\0';
    strcpy(szPoint, reqInfo->args);

    if(!(t = strchr(szPoint,','))) {
        log_reason(reqInfo, 
		   "Your client doesn't support image mapping properly.",
		   reqInfo->filename);
	die(reqInfo, SC_BAD_IMAGEMAP, reqInfo->url);
    }

    *t++ = '\0';
    testpoint[X] = (double) atoi(szPoint);
    testpoint[Y] = (double) atoi(t);

    if(!(fp=FOpen(reqInfo->filename,"r"))){
	log_reason(reqInfo, "File permissions deny server access",
		   reqInfo->filename);
	die(reqInfo, SC_FORBIDDEN, reqInfo->url);
    }

    while (fgets(input,MAX_STRING_LEN,fp)) {
        char type[MAX_STRING_LEN];
        char url[MAX_STRING_LEN];
        char num[10];

        if((input[0] == '#') || (!input[0]))
            continue;

        type[0] = '\0';url[0] = '\0';

        for(i=0;isname(input[i]) && (input[i]);i++)
            type[i] = input[i];
        type[i] = '\0';

        while(isspace(input[i])) ++i;
        for(j=0;input[i] && isname(input[i]);++i,++j)
            url[j] = input[i];
        url[j] = '\0';

        if(!strcmp(type,"default") && !sawpoint) {
            strcpy(def,url);
            continue;
        }

        k=0;
        while (input[i]) {
            while (isspace(input[i]) || input[i] == ',')
                i++;
            j = 0;
            while (isdigit(input[i]))
                num[j++] = input[i++];
            num[j] = '\0';
            if (num[0] != '\0')
                pointarray[k][X] = (double) atoi(num);
            else
                break;
            while (isspace(input[i]) || input[i] == ',')
                i++;
            j = 0;
            while (isdigit(input[i]))
                num[j++] = input[i++];
            num[j] = '\0';
            if (num[0] != '\0')
                pointarray[k++][Y] = (double) atoi(num);
            else {
                FClose(fp);
		log_reason(reqInfo, "Imagemap args missing y value.",
			   reqInfo->filename);
		die(reqInfo, SC_BAD_IMAGEMAP, reqInfo->url);
            }
        }
        pointarray[k][X] = -1;
        if(!strcmp(type,"poly"))
            if(pointinpoly(testpoint,pointarray))
                sendmesg(reqInfo, url, fp);
        if(!strcmp(type,"circle"))
            if(pointincircle(testpoint,pointarray))
                sendmesg(reqInfo, url, fp);
        if(!strcmp(type,"rect"))
            if(pointinrect(testpoint,pointarray))
                sendmesg(reqInfo, url, fp);
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
    if(def[0])
        sendmesg(reqInfo, def, fp);

    FClose(fp);
/* No reason to log each of these as an "error" */
/*    log_reason(reqInfo, "No default defined in imagemap.",
	       reqInfo->filename); */
    die(reqInfo, SC_NO_CONTENT, reqInfo->url);
    return 0;
}

void sendmesg(per_request* reqInfo, char *url, FILE* fp)
{
    char loc[HUGE_STRING_LEN];

    FClose(fp);
    if (!strchr(url, ':')) {   /*** If not a full URL ***/
	if (port == 80)                    /*** It is a virtual URL ***/
	    sprintf(loc, "http://%s", reqInfo->hostInfo->server_hostname);
	else   /* only add port if it's not the default */
	    sprintf(loc, "http://%s:%d",reqInfo->hostInfo->server_hostname, 
		    port);
      strcat(loc,url);
      die(reqInfo,SC_REDIRECT_TEMP,loc);
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

        radius1 = ((coords[0][Y] - coords[1][Y]) * (coords[0][Y] -
        coords[1][Y])) + ((coords[0][X] - coords[1][X]) * (coords[0][X] -
        coords[1][X]));
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

int isname(char c)
{
        return (!isspace(c));
}

#endif /* IMAGEMAP_SUPPORT */
