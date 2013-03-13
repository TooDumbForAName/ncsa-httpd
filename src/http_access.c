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
 * http_access.c,v 1.78 1996/04/05 18:54:49 blong Exp
 *
 ************************************************************************
 * 
 * http_access: Security options etc.
 *
 */


#include "config.h"
#include "portability.h"

#include <stdio.h>
#ifndef NO_STDLIB_H 
# include <stdlib.h>
#endif /* NO_STDLIB_H */
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>
#include <errno.h>
#include "constants.h"
#include "http_access.h"
#include "http_request.h"
#include "http_config.h"
#include "http_auth.h"
#include "http_mime.h"
#include "http_log.h"
#include "util.h"
#include "allocate.h"

/*
 * Modified this bad boy so he wouldn't
 * match things like "allow csa.ncsa.uiuc.edu" and
 * a ncsa.uiuc.edu. SSG 7/10/95
 */
int in_domain(char *domain, char *what) 
{
    int dl=strlen(domain);
    int wl=strlen(what);

    if(((wl-dl) >= 0) && !strcmp(domain,&what[wl-dl])) {
	if (((wl - dl) > 0) && (*domain != '.') && (what[wl-dl-1] != '.')) 
	    return 0;   /* only a partial match - SSG */
	return 1;
    }
    else {
        return 0;
    }
}

/*
 * Modified this OTHER bad boy so he won't match
 * "1.2.3.40" with "allow 1.2.3.4", "1.2.30.4" with "allow 1.2.3", etc.
 * (Documentation implies that these should not match, but they did.)
 * Address matching should really be done with subnet masks, though.
 * mullen@itd.nrl.navy.mil 11/16/95
 *
 * Returned to normal, as the patch didn't work.  For now, I think
 * that updating the documenation to say that you have to end in
 * a . in order to prohibit other networks is better, as some people
 * might like the ability to allow anything that matches 128.174.1
 * blong - 1/26/96
 */
int in_ip(char *domain, char *what) 
{
/*   int dl=strlen(domain);

   return (!strncmp(domain,what,dl)) &&
            (domain[dl-1]=='.' &&  strlen(what)<=dl &&   what[dl]=='.'); */

   return(!strncmp(domain,what,strlen(domain))); 
}

/* find_host_allow()
 * Hunts down list of allowed hosts and returns 1 if allowed, 0 if not
 * As soon as it finds an allow that matches, it returns 1
 */

int find_host_allow(per_request *reqInfo, int x) 
{
    register int y;

    /* If no allows are specified, then allow */
    if(sec[x].num_allow[reqInfo->method] == 0)
        return FA_ALLOW;

    for(y=0;y<sec[x].num_allow[reqInfo->method];y++) {
        if(!strcmp("all",sec[x].allow[reqInfo->method][y])) 
            return FA_ALLOW;
#ifdef LOCALHACK
        if((!strcmp("LOCAL",sec[x].allow[reqInfo->method][y])) &&
                 reqInfo->remote_host &&
		  (ind(reqInfo->remote_host,'.') == -1)) 
	 {
	    reqInfo->bSatisfiedDomain = TRUE;
	    return FA_ALLOW;
         }
#endif /* LOCALHACK */
        if(in_ip(sec[x].allow[reqInfo->method][y],reqInfo->remote_ip))
	  {
            reqInfo->bSatisfiedDomain = TRUE;
	    return FA_ALLOW;
	  }
	/* If we haven't done a lookup, and the DNS type is Minimum, then
	 * we do a lookup now
	 */
	if (!reqInfo->dns_host_lookup && 
	   (reqInfo->hostInfo->dns_mode == DNS_MIN))
	     get_remote_host_min(reqInfo);
        if(reqInfo->remote_host) {
          if(in_domain(sec[x].allow[reqInfo->method][y],reqInfo->remote_host))
	    {
              reqInfo->bSatisfiedDomain = TRUE;
	      return FA_ALLOW;
	    }
        }
    }
    /* Default is to deny */
    return FA_DENY;
}

/* find_host_deny()
 * Hunts down list of denied hosts and returns 0 if denied, 1 if not
 * As soon as it finds a deny that matches, it returns 0
 */
int find_host_deny(per_request *reqInfo, int x) 
{
    register int y;

    /* If there aren't any denies, then it is allowed 
     */
    if(sec[x].num_deny[reqInfo->method] == 0)
        return FA_ALLOW;

    for(y=0;y<sec[x].num_deny[reqInfo->method];y++) {
        if(!strcmp("all",sec[x].deny[reqInfo->method][y])) 
	  {
	    reqInfo->bSatisfiedDomain = FALSE;
	    return FA_DENY;
 	  }
#ifdef LOCALHACK
       if((!strcmp("LOCAL",sec[x].deny[reqInfo->method][y])) &&
		 reqInfo->remote_host &&
                 (ind(reqInfo->remote_host,'.') == -1))
	 {
             reqInfo->bSatisfiedDomain = FALSE;
	     return FA_DENY;
	 }
#endif /* LOCALHACK */
        if(in_ip(sec[x].deny[reqInfo->method][y],reqInfo->remote_ip))
	  {
	    reqInfo->bSatisfiedDomain = FALSE;
	    return FA_DENY;
	  }
	if (!reqInfo->dns_host_lookup && 
	   (reqInfo->hostInfo->dns_mode == DNS_MIN))
	     get_remote_host_min(reqInfo);
        if(reqInfo->remote_host) {
          if(in_domain(sec[x].deny[reqInfo->method][y],reqInfo->remote_host))
	      {
		reqInfo->bSatisfiedDomain = FALSE;
		return FA_DENY;
	      }
	}
    }
    /* Default is to allow */
    reqInfo->bSatisfiedDomain = TRUE;
    return FA_ALLOW;
}

/* match_referer()
 * currently matches restriction with sent for only as long as restricted
 */
int match_referer(char *restrict, char *sent) {
  return !(strcmp_match(sent,restrict));
}

/* find_referer_allow()
 * Hunts down list of allowed hosts and returns 1 if allowed, 0 if not
 * As soon as it finds an allow that matches, it returns 1
 */

int find_referer_allow(per_request *reqInfo, int x)
{
    register int y;

    /* If no allows are specified, then allow */
    if(sec[x].num_referer_allow[reqInfo->method] == 0)
        return FA_ALLOW;

    for(y=0;y<sec[x].num_referer_allow[reqInfo->method];y++) {
        if(!strcmp("all",sec[x].referer_allow[reqInfo->method][y]))
            return FA_ALLOW;
#ifdef LOCALHACK
/* I haven't quite come up with either a reason or a method for
 * using the LOCALHACK with referer, so nothing for now.
 */
#endif /* LOCALHACK */
        if(match_referer(sec[x].referer_allow[reqInfo->method][y],
                         reqInfo->inh_referer))
          {
            reqInfo->bSatisfiedReferer = TRUE;
            return FA_ALLOW;
          }
    }
    /* Default is to deny */
    return FA_DENY;
}

/* find_referer_deny()
 * Hunts down list of denied hosts and returns 0 if denied, 1 if not
 * As soon as it finds a deny that matches, it returns 0
 */
int find_referer_deny(per_request *reqInfo, int x)
{
    register int y;

    /* If there aren't any denies, then it is allowed
     */
    if(sec[x].num_referer_deny[reqInfo->method] == 0)
        return FA_ALLOW;

    for(y=0;y<sec[x].num_referer_deny[reqInfo->method];y++) {
        if(!strcmp("all",sec[x].referer_deny[reqInfo->method][y]))
          {
            reqInfo->bSatisfiedReferer = FALSE;
            return FA_DENY;
          }
#ifdef LOCALHACK
/* I haven't quite come up with either a reason or a method for
 * using the LOCALHACK with referer, so nothing for now.
 */
#endif /* LOCALHACK */
        if(match_referer(sec[x].referer_deny[reqInfo->method][y],
		         reqInfo->inh_referer))
          {
            reqInfo->bSatisfiedReferer = FALSE;
            return FA_DENY;
          }
    }
    /* Default is to allow */
    reqInfo->bSatisfiedReferer = TRUE;
    return FA_ALLOW;
}



void check_dir_access(per_request *reqInfo,int x, 
		      int *allow, int *allow_options, int *other) 
{
    if(sec[x].auth_name[0]) 
	reqInfo->auth_name = sec[x].auth_name;
    if(sec[x].auth_pwfile[0]) {
        reqInfo->auth_pwfile = sec[x].auth_pwfile;
	reqInfo->auth_pwfile_type = sec[x].auth_pwfile_type;
    }
    if(sec[x].auth_grpfile[0]) {
        reqInfo->auth_grpfile = sec[x].auth_grpfile;
        reqInfo->auth_grpfile_type = sec[x].auth_grpfile_type;
    }
#ifdef DIGEST_AUTH
    if(sec[x].auth_digestfile[0]) {
        reqInfo->auth_digestfile = sec[x].auth_digestfile;
        reqInfo->auth_digestfile_type = sec[x].auth_digestfile_type;
    }
#endif /* DIGEST_AUTH */
    if (sec[x].auth_type[0])
	strcpy(reqInfo->auth_type,sec[x].auth_type);

    if(sec[x].order[reqInfo->method] == ALLOW_THEN_DENY) {
        *allow=FA_DENY;
	if ((find_host_allow(reqInfo,x) == FA_ALLOW) && 
	    (find_referer_allow(reqInfo,x) == FA_ALLOW))
           *allow = FA_ALLOW;
	if ((find_host_deny(reqInfo,x) == FA_DENY) ||
	    (find_referer_deny(reqInfo,x) == FA_DENY))
           *allow = FA_DENY;
    } 
    else if(sec[x].order[reqInfo->method] == DENY_THEN_ALLOW) {
	if ((find_host_deny(reqInfo,x) == FA_DENY) ||
	    (find_referer_deny(reqInfo,x) == FA_DENY))
           *allow = FA_DENY;
	if ((find_host_allow(reqInfo,x) == FA_ALLOW) &&
	    (find_referer_allow(reqInfo,x) == FA_ALLOW))
           *allow = FA_ALLOW;
    }
    else { /* order == MUTUAL_FAILURE: allowed and not denied */
      *allow = FA_DENY;
      if ((find_host_allow(reqInfo,x) == FA_ALLOW) &&
	  (find_referer_allow(reqInfo,x) == FA_ALLOW) &&
          !(find_host_deny(reqInfo,x) == FA_DENY) &&
	  !(find_referer_deny(reqInfo,x) == FA_DENY))
            *allow = FA_ALLOW;
    }
   
    if(sec[x].num_auth[reqInfo->method])
        *allow_options=x;
}

void evaluate_access(per_request *reqInfo,struct stat *finfo,int *allow,
		     char *allow_options) 
{
    int will_allow, need_auth, num_dirs;
    int need_enhance;
    char opts[MAX_STRING_LEN], override[MAX_STRING_LEN];
    char path[MAX_STRING_LEN], d[MAX_STRING_LEN];
    char errstr[MAX_STRING_LEN];
    register int x,y,z,n;

    if(S_ISDIR(finfo->st_mode)) 
      strncpy_dir(path,reqInfo->filename,MAX_STRING_LEN);
    else lim_strcpy(path,reqInfo->filename,MAX_STRING_LEN);
    
    no2slash(path);

    num_dirs = count_dirs(path);
    will_allow = FA_ALLOW; 
    need_auth = -1;
    need_enhance = -1;

    reqInfo->auth_user[0] = '\0';
    reqInfo->auth_group[0] = '\0';
    reset_mime_vars();

    for(x=0;x<num_dirs;x++) {
        opts[x] = OPT_ALL;
        override[x] = OR_ALL;
    }

    /* assume not domain restricted */
    reqInfo->bSatisfiedDomain = 0;
    
    n=num_dirs-1;
    for(x=0;x<num_sec;x++) {
        if(is_matchexp(sec[x].d)) {
            if(!strcmp_match(path,sec[x].d)) {
                for(y=0;y<num_dirs;y++) {
                    if(!(sec[x].opts & OPT_UNSET))
                        opts[y] = sec[x].opts;
                    override[y] = sec[x].override;
                }
                check_dir_access(reqInfo,x,&will_allow,&need_auth,&need_enhance);
            }
        }
        else if(!strncmp(path,sec[x].d,strlen(sec[x].d))) {
            for(y=count_dirs(sec[x].d) - 1;y<num_dirs;y++) {
                if(!(sec[x].opts & OPT_UNSET))
                    opts[y] = sec[x].opts;
                override[y] = sec[x].override;
            }
            check_dir_access(reqInfo,x,&will_allow,&need_auth,&need_enhance);
        }
	if (!will_allow && sec[x].on_deny[reqInfo->method]) {
	  strcpy(reqInfo->outh_location,sec[x].on_deny[reqInfo->method]);
        }
    }
    if((override[n]) || (!(opts[n] & OPT_SYM_LINKS)) || 
       (opts[n] & OPT_SYM_OWNER)) {
	for(x=0;x<num_dirs;x++) {
	    y = num_sec;
	    make_dirstr(path,x+1,d);
	    if((!(opts[x] & OPT_SYM_LINKS)) || (opts[x] & OPT_SYM_OWNER)) {
		struct stat lfi,fi;
		
		if(lstat(d,&lfi) != 0)
		{
		    sprintf(errstr,"HTTPd: can't lstat %s, errno = %d",d, errno);
		    log_error(errstr,reqInfo->hostInfo->error_log);
		    *allow=FA_DENY;
		    *allow_options = OPT_NONE;
		    return;
		}
		if(!(S_ISDIR(lfi.st_mode))) {
		    if(opts[x] & OPT_SYM_OWNER) {
			if(stat(d,&fi) != 0)
			{
			    sprintf(errstr,"HTTPd: can't stat %s, errno = %d",d, errno);
			    log_error(errstr,reqInfo->hostInfo->error_log);
			    *allow=FA_DENY;
			    *allow_options = OPT_NONE;
			    return;
			}
			if(fi.st_uid != lfi.st_uid)
			    goto bong;
		    }
		    else {
		      bong:
			sprintf(errstr,"HTTPd: will not follow link %s",d);
			log_error(errstr,reqInfo->hostInfo->error_log);
			*allow=FA_DENY;
			*allow_options = OPT_NONE;
			return;
		    }
		}
	    }
	    if(override[x]) {
		parse_htaccess(reqInfo,d,override[x]);
		if(num_sec != y) {
		    for(z=count_dirs(sec[y].d) - 1;z<num_dirs;z++) {
			if(!(sec[y].opts & OPT_UNSET))
			    opts[z] = sec[y].opts;
			override[z] = sec[y].override;
		    }
		    if ((sec[y].num_auth[reqInfo->method] > 0) || 
			(sec[y].num_allow[reqInfo->method] > 0) ||
			(sec[y].num_deny[reqInfo->method] > 0) ||
			(sec[y].num_referer_allow[reqInfo->method] > 0) ||
			(sec[y].num_referer_deny[reqInfo->method] > 0))
			check_dir_access(reqInfo,y,&will_allow,&need_auth,&need_enhance);
	                if (!will_allow && sec[y].on_deny[reqInfo->method]) {
	                  strcpy(reqInfo->outh_location,
				 sec[y].on_deny[reqInfo->method]);
       			} 
		}
	    }
	}
    }
    if((!(S_ISDIR(finfo->st_mode))) && 
       ((!(opts[n] & OPT_SYM_LINKS)) || (opts[n] & OPT_SYM_OWNER))) {
        struct stat fi,lfi;
        if(lstat(path,&fi)!=0)
	{
	    sprintf(errstr,"HTTPd: can't lstat %s, errno = %d",path, errno);
	    log_error(errstr,reqInfo->hostInfo->error_log);
	    *allow=FA_DENY;
	    *allow_options = OPT_NONE;
	    return;
	}
        if(!(S_ISREG(fi.st_mode))) {
            if(opts[n] & OPT_SYM_OWNER) {
                if(stat(path,&lfi)!=0)
		{
		    sprintf(errstr,"HTTPd: can't stat %s, errno = %d",path, errno);
		    log_error(errstr,reqInfo->hostInfo->error_log);
		    *allow=FA_DENY;
		    *allow_options = OPT_NONE;
		    return;
		}
                if(fi.st_uid != lfi.st_uid)
                    goto gong;
            }
            else {
              gong:
                sprintf(errstr,"HTTPd: will not follow link %s",path);
                log_error(errstr,reqInfo->hostInfo->error_log);
                *allow=FA_DENY;
                *allow_options = OPT_NONE;
                return;
            }
        }
    }
    *allow = will_allow;
    if(will_allow) {
        *allow_options = opts[num_dirs-1];
        if ((need_auth >= 0) && (sec[need_auth].bSatisfy == SATISFY_ALL)) {
	    reqInfo->bSatisfiedDomain = 0;
	    check_auth(reqInfo,&sec[need_auth], reqInfo->inh_auth_line);
	}
    } else if ((need_auth >= 0) && (sec[need_auth].bSatisfy == SATISFY_ANY)) {
	check_auth(reqInfo,&sec[need_auth], reqInfo->inh_auth_line);
	*allow_options = opts[num_dirs-1];
	*allow = FA_ALLOW;
    }
    else *allow_options = 0;
}

void kill_security(void) 
{
    register int x,y,m;

    for(x=0;x<num_sec;x++) {
        /*free(sec[x].d);*/
        for(m=0;m<METHODS;m++) {
            for(y=0;y<sec[x].num_allow[m];y++)
                free(sec[x].allow[m][y]);
            for(y=0;y<sec[x].num_deny[m];y++)
                free(sec[x].deny[m][y]);
            for(y=0;y<sec[x].num_auth[m];y++)
                free(sec[x].auth[m][y]);
            for(y=0;y<sec[x].num_referer_allow[m];y++)
		free(sec[x].referer_allow[m][y]);
            for(y=0;y<sec[x].num_referer_deny[m];y++)
		free(sec[x].referer_deny[m][y]);
            free(sec[x].on_deny[m]);
        }
/*        if(sec[x].auth_type)
            free(sec[x].auth_type);
        if(sec[x].auth_name)
            free(sec[x].auth_name);
        if(sec[x].auth_pwfile)
            free(sec[x].auth_pwfile);
        if(sec[x].auth_grpfile)
            free(sec[x].auth_grpfile);
#ifdef DIGEST_AUTH
        if(sec[x].auth_digestfile)
            free(sec[x].auth_digestfile);
#endif*/ /* DIGEST_AUTH */
    }
}


/* This function should reset the security data structure to contain only
   the information given in the access configuration file.  It should be 
   called after any transactions */

void reset_security(void) 
{
    register int x,y,m;

    for(x=num_sec_config;x<num_sec;x++) {
        /*free(sec[x].d);*/
        for(m=0;m<METHODS;m++) {
            for(y=0;y<sec[x].num_allow[m];y++)
                free(sec[x].allow[m][y]);
            for(y=0;y<sec[x].num_deny[m];y++)
                free(sec[x].deny[m][y]);
            for(y=0;y<sec[x].num_auth[m];y++)
                free(sec[x].auth[m][y]);
            for(y=0;y<sec[x].num_referer_allow[m];y++)
		free(sec[x].referer_allow[m][y]);
            for(y=0;y<sec[x].num_referer_deny[m];y++)
		free(sec[x].referer_deny[m][y]);
            if (sec[x].on_deny[m]) free(sec[x].on_deny[m]);
        }
/*        if(sec[x].auth_type)
            free(sec[x].auth_type);
        if(sec[x].auth_name)
            free(sec[x].auth_name);
        if(sec[x].auth_pwfile)
            free(sec[x].auth_pwfile);
        if(sec[x].auth_grpfile)
            free(sec[x].auth_grpfile);
#ifdef DIGEST_AUTH
        if(sec[x].auth_digestfile)
            free(sec[x].auth_digestfile);
#endif*/ /* DIGEST_AUTH */
    }

   num_sec = num_sec_config;
}

