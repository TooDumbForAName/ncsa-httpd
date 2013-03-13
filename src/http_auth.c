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
 * http_auth: authentication
 *
 * Based on NCSA HTTPd 1.3 by Rob McCool
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
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#ifdef DBM_SUPPORT
# ifndef _DBMSUPPORT_H  /* moronic OSs which don't protect their own include */
#  define _DBMSUPPORT_H  /* files from being multiply included */
#  include <ndbm.h>
# endif /* _DBMSUPPORT_H */
#endif /* DBM_SUPPORT */

#ifdef NIS_SUPPORT
#include <rpcsvc/ypclnt.h>
#endif /* NIS_SUPPORT */

#if defined(KRB4) || defined(KRB5)
# define HAVE_KERBEROS
#endif /* defined(KRB4) || defined(KRB5) */
#ifdef KRB4
# include <krb.h>
#endif /* KRB4 */
#ifdef KRB5
# include <krb5.h>
#endif /* KRB5 */

#include "constants.h"
#include "fdwrap.h"
#include "http_auth.h" 
#include "http_access.h" 
#include "http_mime.h"
#include "http_config.h" 
#include "http_log.h"
#include "util.h"
#include "digest.h"


char user[MAX_STRING_LEN]; 
char groupname[MAX_STRING_LEN];


#ifdef HAVE_KERBEROS
#define T 1
#define NIL 0
char* index();
char krb_authreply[2048];
extern char *remote_logname; 
extern char out_auth_header[];

/* Table for converting binary values to and from hexadecimal */
static char hex[] = "0123456789abcdef";
static char dec[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /*   0 -  15 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /*  16 -  37 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /* ' ' - '/' */
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0,   /* '0' - '?' */
    0,10,11,12,13,14,15, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /* '@' - 'O' */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /* 'P' - '_' */
    0,10,11,12,13,14,15, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /* '`' - 'o' */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /* 'p' - DEL */
};
#endif /* HAVE_KERBEROS */

#ifdef KRB4
#define MAX_KDATA_LEN MAX_KTXT_LEN
char k4_srvtab[MAX_STRING_LEN] = "";
static des_cblock session;      /* Our session key */
static des_key_schedule schedule; /* Schedule for our session key */
AUTH_DAT kerb_kdata;
#endif /* KRB4 */

#ifdef KRB5
#ifndef MAX_KDATA_LEN
#define MAX_KDATA_LEN 2048     
#endif /* MAX_KDATA_LEN */
char k5_srvtab[MAX_STRING_LEN] = "";
#endif /* KRB5 */

#ifdef NIS_SUPPORT
int
init_nis(char **dom)
{
      static int      init = 0;
      static char     *domain;
      int             yperr;

      if (init == 0) {
              yperr = yp_get_default_domain(&domain);
              if (yperr == 0)
                      init++;
      }

      if (init) {
              *dom = domain;
              return 0;
      }
      return 1;
}
#endif /* NIS_SUPPORT */

int get_pw(per_request *reqInfo, char *user, char *pw, security_data* sec) 
{ 
    FILE *f; 
    char errstr[MAX_STRING_LEN]; 
    char l[MAX_STRING_LEN];
    char w[MAX_STRING_LEN]; 
    struct stat finfo;

    if (reqInfo->auth_pwfile_type == AUTHFILETYPE_STANDARD) { 	
	/* From Conrad Damon (damon@netserver.standford.edu), 	 
	   Don't start cfg_getline loop if auth_pwfile is a directory. */

	if ((stat (reqInfo->auth_pwfile, &finfo) == -1) ||
	    (!S_ISREG(finfo.st_mode))) {
	    sprintf(errstr,"%s is not a valid file.", reqInfo->auth_pwfile);
	    die(reqInfo,SC_SERVER_ERROR,errstr);
	}

	if(!(f=FOpen(reqInfo->auth_pwfile,"r"))) { 	 
	    sprintf(errstr,"Could not open user file %s",reqInfo->auth_pwfile);
	    die(reqInfo,SC_SERVER_ERROR,errstr); 	
	}
	while(!(cfg_getline(l,MAX_STRING_LEN,f))) { 	 
	    if((l[0] == '#') || (!l[0])) 
		continue; 	 
	    getword(w,l,':');
	    if(!strcmp(user,w)) {
 		strcpy(pw,l);
 		FClose(f);
		return 1;
	    }
 	}
 	FClose(f);
 	return 0;
    } 
#ifdef DBM_SUPPORT
    else if(reqInfo->auth_pwfile_type == AUTHFILETYPE_DBM) {
 	DBM* db;
 	datum dtKey, dtRec;

	if(!(db = DBM_Open(reqInfo->auth_pwfile,O_RDONLY, 0))) {
	    sprintf(errstr,"Could not open user file %s",reqInfo->auth_pwfile);
	    die(reqInfo,SC_SERVER_ERROR,errstr);
 	}
	dtKey.dptr = user;
 	dtKey.dsize = strlen(user);
 	dtRec = dbm_fetch(db, dtKey);
 	DBM_Close(db);
 	if (dtRec.dptr) {
  	    strncpy(pw, dtRec.dptr, dtRec.dsize);
            pw[dtRec.dsize] = '\0';
	    return 1; 	
	}
 	else
	    return 0; 
    } 
#endif /* DBM_SUPPORT */
#ifdef NIS_SUPPORT
    else if (reqInfo->auth_pwfile_type == AUTHFILETYPE_NIS) {
      char    *domain,
              *pwfile,
              *resptr;
      int     yperr,
              resize;

      if (init_nis(&domain) != 0)
              return 0;

      if (strcmp(reqInfo->auth_pwfile, "+"))
              pwfile = reqInfo->auth_pwfile;
      else
              pwfile = "passwd.byname";

      yperr = yp_match(domain, pwfile, user, strlen(user), &resptr, &resize);
      if (yperr == 0) {
              getword(w, resptr, ':');
              if (strcmp(w, user) == 0) {
                      getword(w, resptr, ':');
                      (void) strcpy(pw, w);
                      return 1;
              }
      }
      return 0;
    } 
#endif /* NIS_SUPPORT */
    else
	die(reqInfo,SC_SERVER_ERROR,"Invalid password file type"); 
    return 0; 
}

int in_group(per_request *reqInfo, char *user, 
	     char *group, char* pchGrps
#ifdef DBM_SUPPORT
	     , DBM* db 
#endif /* DBM_SUPPORT */
) {
    char *mems = NULL, *endp = NULL;
    char *pch;
    char chSaved = '\0';
    int  nlen, bFound = 0;
    char l[MAX_STRING_LEN];
    int x,start;

    if (reqInfo->auth_grpfile_type == AUTHFILETYPE_STANDARD) {
	nlen = strlen (group);
	if ((mems = strstr (pchGrps, group)) && *(mems + nlen) == ':') {
	    if ((endp = strchr (mems + nlen + 1, ':'))) {
		while (!isspace(*endp)) endp--;
		chSaved = *endp;
		*endp = '\0';
	    }
/* BUG FIX: Couldn't have the same name as the group as a user because
 * failed to move the string beyond the group:
 */
            mems = mems + nlen;
	}
	else
	    return 0;
    }
#ifdef DBM_SUPPORT
    else if (reqInfo->auth_grpfile_type == AUTHFILETYPE_DBM) {
	datum dtKey, dtRec;

	dtKey.dptr = group;
 	dtKey.dsize = strlen(group);
 	dtRec = dbm_fetch(db, dtKey);
 	if (dtRec.dptr) {
           strncpy(l, dtRec.dptr, dtRec.dsize);
           l[dtRec.dsize] = '\0';
           mems = l;
	}
 	else
	    return 0;
    }
#endif /* DBM_SUPPORT */
#ifdef NIS_SUPPORT
    else if (reqInfo->auth_pwfile_type == AUTHFILETYPE_NIS) {
      char    *domain,
              *grfile,
              *resptr,
              w[MAX_STRING_LEN];
      int     yperr,
              resize;

      if (init_nis(&domain) != 0)
              return 0;

      if (strcmp(reqInfo->auth_grpfile, "+"))
              grfile = reqInfo->auth_grpfile;
      else
              grfile = "webgroup";

      yperr = yp_match(domain, grfile, group, strlen(group), &resptr, &resize);
      if (yperr != 0)
              return 0;

      getword(w, resptr, ':');
      if (strcmp(w, group) != 0)
              return 0;

      while (*resptr && isspace(*resptr))
              resptr++;
      (void) strcpy(l, resptr);
      mems = l;
    } 
#endif /* NIS_SUPPORT */
    else {
      die(reqInfo,SC_SERVER_ERROR,"Invalid group file type");
      return 0;
    }

/* Actually search the group line for the user.  Can be comma or space
 * delimited
 */
    x = 0;
    start = 0;
    pch = mems;
    while (!bFound && (pch[x] != '\0')) {
      if ((isspace(pch[x])) || (pch[x] == ',')) {
        bFound = !strncmp(user,(pch+start),x-start);
        start = x+1;
      }
      x++;
    }
    if (!bFound && pch[x] == '\0' && (x-start > 0)) {
      bFound = !strncmp(user,(pch+start),x-start);
    }
 

/* Buggy, and obfuscated.  */
/*    nlen = strlen (user);
    nlen = strlen (user);
    pch = mems;
    while (!bFound && (pch = strstr(pch, user)) && 
       (!*(pch + nlen) || isspace (*(pch + nlen)) || *(pch + nlen) == ','))
       bFound = 1; 
*/
    if (endp && *endp == '\0') *endp = chSaved;
    return bFound;
}

char* init_group(per_request *reqInfo,char* grpfile) 
{ 
    FILE *fp; 
    char* chData;
    int   nSize;
    struct stat finfo;

    if ((stat (grpfile, &finfo) == -1) || (!S_ISREG(finfo.st_mode))) {
	return 0; 
    }

    if(!(fp=FOpen(grpfile,"rb"))) 
	return 0;

    fseek (fp, 0, 2);  /* seek to end */
    nSize = ftell (fp);
    rewind (fp);

    nSize++;            /* add room for null char */
    if (!(chData = (char*) malloc (nSize * sizeof (char)))) {
	FClose (fp);
        log_error("Not enough memory to read group file",
		  reqInfo->hostInfo->error_log);
	return NULL;
    }

    fread (chData, nSize - 1, 1, fp);
    chData[nSize - 1] = '\0';
    FClose(fp); 
    return chData;
}

void auth_bong(per_request *reqInfo,char *s,char* auth_name, char* auth_type) 
{
    char errstr[MAX_STRING_LEN];

    /* debugging */
    if (s) {
        sprintf(errstr,"%s authorization: %s",reqInfo->remote_name,s);
        log_error(errstr,reqInfo->hostInfo->error_log);
    }
    if(!strcasecmp(auth_type,"Basic")) {
        sprintf(errstr,"Basic realm=\"%s\"",auth_name);
        die(reqInfo,SC_AUTH_REQUIRED,errstr);
    }
#ifdef DIGEST_AUTH
    if(!strcasecmp(auth_type,"Digest")) {
      Digest_Construct401(reqInfo,errstr,0,auth_name);
      die(reqInfo,SC_AUTH_REQUIRED,errstr);
    }
#endif /* DIGEST_AUTH */
#ifdef KRB4
    if(!strncasecmp(auth_type,"KerberosV4",10)) {
        sprintf(errstr,"KerberosV4");
        die(reqInfo,SC_AUTH_REQUIRED,errstr);
    }
#endif /* KRB4 */
#ifdef KRB5
    if(!strncasecmp(auth_type,"KerberosV5",10)) {
        sprintf(errstr,"KerberosV5");
        die(reqInfo,SC_AUTH_REQUIRED,errstr);
    }
#endif /* KRB5 */
    else {
        sprintf(errstr,"Unknown authorization method %s",auth_type);
        die(reqInfo,SC_SERVER_ERROR,errstr);
    }
}

void check_auth(per_request *reqInfo, security_data *sec, char* auth_line) 
{
    char ad[MAX_STRING_LEN];
    char w[MAX_STRING_LEN];
    char t[MAX_STRING_LEN];
    char sent_pw[MAX_STRING_LEN];
    char real_pw[MAX_STRING_LEN];
    char errstr[MAX_STRING_LEN];
    char auth_type[MAX_STRING_LEN];
    register int x,y;
    int grpstatus;
    int bValid;         /* flag to indicate authorized or not */
    char* pchGrpData = NULL;
#ifdef DBM_SUPPORT
    DBM* db = NULL;
#endif /* DBM_SUPPORT */
#ifdef HAVE_KERBEROS
    int krbresult;
    char* kauth_line;
    KerberosInfo kdat;
#endif /* HAVE_KERBEROS */

    if(!sec->auth_type[0])
	strcpy(sec->auth_type, "Basic");
    
    if(!auth_line[0])
	auth_bong(reqInfo,NULL, reqInfo->auth_name, sec->auth_type);

    for (x=0 ; auth_line[x] && (auth_line[x] != ' ') && x < MAX_STRING_LEN; x++)
	auth_type[x] = auth_line[x];
    auth_type[x++] = '\0';

    if (strcmp(auth_type, sec->auth_type))
	auth_bong(reqInfo,"type mismatch",reqInfo->auth_name,sec->auth_type);

    if(!strcasecmp(sec->auth_type,"Basic")) {
        if(!reqInfo->auth_name) {
            sprintf(errstr,"httpd: need AuthName for %s",sec->d);
            die(reqInfo,SC_SERVER_ERROR,errstr);
        }
        if(!reqInfo->auth_pwfile) {
            sprintf(errstr,"httpd: need AuthUserFile for %s",sec->d);
            die(reqInfo,SC_SERVER_ERROR,errstr);
        }

        uudecode(auth_line + strlen(auth_type),(unsigned char *)ad,MAX_STRING_LEN);
        getword(user,ad,':');
        strcpy(sent_pw,ad);
        if(!get_pw(reqInfo,user,real_pw,sec)) {
            sprintf(errstr,"user %s not found",user);
            auth_bong(reqInfo,errstr,reqInfo->auth_name,sec->auth_type);
        }
        /* anyone know where the prototype for crypt is? */
	/* Yeah, in unistd.h on most systems, it seems */
        if(strcmp(real_pw,(char *)crypt(sent_pw,real_pw))) {
            sprintf(errstr,"user %s: password mismatch",user);
            auth_bong(reqInfo,errstr,reqInfo->auth_name,sec->auth_type);
        }
    }
#ifdef DIGEST_AUTH
    else if(!strcasecmp(sec->auth_type,"Digest")) {
        if(!reqInfo->auth_name) {
            sprintf(errstr,"httpd: need AuthName for %s",sec->d);
            die(reqInfo,SC_SERVER_ERROR,errstr);
        }
        if(!sec->auth_digestfile) {
            sprintf(errstr,"httpd: need AuthDigestFile for %s",sec->d);
            die(reqInfo,SC_SERVER_ERROR,errstr);
        }
        Digest_Check(reqInfo,user, sec);
    }
#endif /* DIGEST_AUTH */
#ifdef HAVE_KERBEROS 
    else if (!strncasecmp(sec->auth_type, "Kerberos", 8)) {

	kauth_line = auth_line + strlen(auth_type);
	while (*kauth_line == ' ')
	    kauth_line++;

	/* now kauth_line should consist of "user <ticket>" */

	if (0) {
	}
#ifdef KRB4
	else if (!strncasecmp(sec->auth_type, "KerberosV4", 10)) {
	    krbresult = k4_server_auth(kauth_line, krb_authreply, 
				       reqInfo->hostInfo->error_log, &kdat);
	}
#endif /* KRB4 */
#ifdef KRB5
	else if (!strncasecmp(sec->auth_type, "KerberosV5", 10)) {
	    krbresult = k5_server_auth(kauth_line, krb_authreply, &kdat);
        }
#endif /* KRB5 */
	else {
	    auth_bong(reqInfo,"auth type not supported",reqInfo->auth_name,
		      sec->auth_type);
	}
    
	if (krbresult) {
	    if (check_krb_restrict(reqInfo, sec, &kdat)) {
		remote_logname = user;
		out_auth_header[0] = '\0'; 
		sprintf(out_auth_header, "WWW-Authenticate: %s %s\r\n", 
			sec->auth_type, krb_authreply);
		return;
	    }
	    else {
		sprintf(errstr, "Kerberos Ok. Denied by access configuration");
		log_error (errstr, reqInfo->hostInfo->error_log);
		auth_bong (reqInfo, "Denied by access configuration", 
			   reqInfo->auth_name,sec->auth_type);
	    }
	} else {
	    sprintf(errstr,"Kerberos_server_auth_error: %s",krb_authreply);
	    log_error(errstr,reqInfo->hostInfo->error_log);
	    auth_bong(reqInfo,"Bad Kerberos credentials submitted",
		      reqInfo->auth_name,sec->auth_type);
	}
    }
#endif /* HAVE_KERBEROS */
    else {
        sprintf(errstr,"unknown authorization type %s for %s",sec->auth_type,
                sec->d);
        auth_bong(reqInfo,errstr,reqInfo->auth_name,sec->auth_type);
    }

    /* Common stuff: Check for valid user */
    grpstatus = 0;
    if(reqInfo->auth_grpfile) {
	if (reqInfo->auth_grpfile_type == AUTHFILETYPE_STANDARD &&
	    (pchGrpData = init_group(reqInfo,reqInfo->auth_grpfile)))
	    grpstatus = 1;
#ifdef DBM_SUPPORT
	else if (reqInfo->auth_grpfile_type == AUTHFILETYPE_DBM) {
	    if((db = DBM_Open(reqInfo->auth_grpfile, O_RDONLY, 0))) {
		grpstatus = 1;
	    }
	} 
#endif /* DBM_SUPPORT */
    }

    bValid = 0;
    for(x=0;x<sec->num_auth[reqInfo->method] && !bValid;x++) {
        strcpy(t,sec->auth[reqInfo->method][x]);
        getword(w,t,' ');
        if(!strcmp(w,"valid-user")) {
	    bValid = 1;
        } 
        else if(!strcmp(w,"user")) {
            while(t[0]) {
                if(t[0] == '\"') {
                    getword(w,&t[1],'\"');
                    for(y=0;t[y];y++)
                        t[y] = t[y+1];
                }
                getword(w,t,' ');
                if(!strcmp(user,w)) {
		    bValid = 1;
		    break;
		}
            }
        }
        else if(!strcmp(w,"group")) {
            if(!grpstatus) {
                sprintf(errstr,"group required for %s, bad groupfile",
                        sec->d);
                auth_bong(reqInfo,errstr,reqInfo->auth_name,sec->auth_type);
            }
            while (t[0]) {
                getword(w,t,' ');
#ifdef DBM_SUPPORT
                if (in_group(reqInfo,user,w, pchGrpData, db)) {
#else
		if (in_group(reqInfo,user,w, pchGrpData)) {
#endif /* DBM_SUPPORT */
		    strcpy(groupname,w);
		    bValid = 1;
		    break;
		}
            }
        }
        else
            auth_bong(reqInfo,"require not followed by user or group",
		      reqInfo->auth_name,sec->auth_type);
    }

    if(grpstatus) {
	if (reqInfo->auth_grpfile_type == AUTHFILETYPE_STANDARD)
	    if (pchGrpData != NULL) free (pchGrpData);
#ifdef DBM_SUPPORT
	else if (reqInfo->auth_grpfile_type == AUTHFILETYPE_DBM)
	    DBM_Close(db);
#endif /* DBM_SUPPORT */
    }
    /* if we didn't validate the user */
    if (!bValid) {
	sprintf(errstr,"user %s denied",user);
	auth_bong(reqInfo,errstr,reqInfo->auth_name,sec->auth_type);
    }
}


#ifdef HAVE_KERBEROS

/*************************************************************************
 * kdata_to_str -- convert 8-bit char array to ascii string
 *
 * Accepts:  input array and length
 * Returns:  a pointer to the result, or null pointer on malloc failure
 *           The caller is responsible for freeing the returned value.
 *
 * Changed to accomodate general strings with length, due to conflict between
 *    KTEXT and krb5_data types  ( 6/28/95 ADC)
 ************************************************************************/
static char *kdata_to_str(in_data, length)
    char *in_data;                      /* char FAR ?? */
    int length;
{
    char *result, *p;
    int i;

    p = result = malloc(length*2+1);
    if (!result) return (char *) NULL;

    for (i=0; i < length; i++) {
        *p++ = hex[(in_data[i]>>4)&0xf];
        *p++ = hex[(in_data[i])&0xf];
    }
    *p++ = '\0';
    return result;
}


/*************************************************************************
 * str_to_kdata -- Converts ascii string to a (binary) char array
 *
 * Accepts: string to convert
 *          pointer to output array
 * Returns: length of output array, NIL on failure
 ************************************************************************/
int str_to_kdata(in_str, out_str)
    char *in_str;
    char *out_str;
{
    int inlen, outlen;

    inlen = strlen(in_str);
    if (inlen & 1) return NIL;  /* must be even number, in this scheme */
    inlen /= 2;
    if (inlen > MAX_KDATA_LEN) return NIL;

    for (outlen=0; *in_str; outlen++, in_str += 2) {
        out_str[outlen] = (dec[in_str[0]]<<4) + dec[in_str[1]];
    }
    return outlen;
}

/*************************************************************************
 * kerberos_server_auth -- Kerberos-authenticated server log in
 * Accepts: user name string
 *          password string
 *          pointer to char pointer.  The char pointer is set to the
 *               text we want returned in the reply message.
 * Returns: T if login ok, NIL otherwise
 ************************************************************************/
#ifdef KRB4
int k4_server_auth(char* authline, char* reply,FILE* error_log, 
		   KerberosInfo *kdat)
{
    char pass[HUGE_STRING_LEN];
    int code;
    KTEXT_ST authent;
    char instance[INST_SZ];
    static AUTH_DAT kdata;
    char realm[REALM_SZ];
    char local_realm[REALM_SZ];
    char *p;
    
    getword(user, authline, ' ');
    getword(pass, authline, '\0');
    

    /* Convert pass to authent */
    if ((authent.length = str_to_kdata(pass, authent.dat)) == NIL) {
	strcpy(reply,"Invalid Kerberos authenticator");
	return NIL;
    }
 
    /* Verify authenticator */
    strcpy(instance, "*");	/* is this ok? */
    if (k4_srvtab[0]) {
        code = krb_rd_req(&authent, "khttp", instance, 0L, &kdata, k4_srvtab);
    }
    else {
        code = krb_rd_req(&authent, "khttp", instance, 0L, &kdata, NULL);
    }

    if (code) {
	sprintf(reply, krb_err_txt[code]);
	log_error(reply,error_log);
	return NIL;
    }

    /* Check authorization of the Kerberos user */
    if (strncmp(kdata.pname, user, ANAME_SZ)) {
	strcpy(reply, "Permission denied; name/username mismatch.");
	return NIL;
    }

    if (code = krb_get_lrealm(local_realm, 1)) {
	sprintf(reply, krb_err_txt[code]);
	log_error(reply, error_log);
	return NIL;
    }

    /* to perform further restriction through .htaccess in check_auth */
    strcpy (kdat->client_name, kdata.pname);
    strcpy (kdat->client_realm, kdata.prealm);
    strcpy (kdat->server_realm, local_realm);
    kdat->ver = KERBEROSV4;

    /* gacck: compat. with older kerb code */
    memcpy(&kerb_kdata, &kdata, sizeof(kdata));

    /* Save the session key */
    bcopy(kdata.session, session, sizeof(des_cblock));
    key_sched(session, schedule);
   
    /* Construct the response for mutual authentication */
    authent.length = sizeof(des_cblock);
    bzero(authent.dat, sizeof(des_cblock));
    *((long *)authent.dat) = htonl(kdata.checksum + 1);
    des_ecb_encrypt(authent.dat, authent.dat, schedule, 1);

    /* Convert response to string and place in buffer */
    p = kdata_to_str(&authent.dat, authent.length);

    if (p) {
	*reply = '[';
	strcpy(reply+1, p);
	strcat(reply, "] User ");
	strcat(reply, user);
	strcat(reply, " authenticated");
	free(p);
    }
    else {
	/* XXX Out of memory */
	exit(1);
    }

    strncpy(user, user, MAX_STRING_LEN - 1);
    return T;
}
#endif	/* KRB4 */
/**********************************************************************/
#ifdef KRB5
int k5_server_auth(char* authline, char* reply, KerberosInfo *kdat)
{
    char pass[HUGE_STRING_LEN];
    char tmpstr[MAX_KDATA_LEN];
    char *p;

    krb5_context k5context;
    krb5_auth_context *k5auth_context = NULL;
    krb5_principal serverp, clientp;
    krb5_data k5authent;
    krb5_ticket *k5ticket = NULL;
    krb5_error_code code;
    krb5_keytab k5keytabid = NULL;
    krb5_data k5ap_rep_data;


    getword(user, authline, ' ');
    getword(pass, authline, '\0');

    /* Convert pass to authent */
    if ((k5authent.length = str_to_kdata(pass, tmpstr)) == NIL) {
        sprintf(reply, "Invalid authenticator");
        return NIL;
    }
    k5authent.data = tmpstr;

    code = krb5_init_context(&k5context);
    if (code) {
        sprintf(reply, "krb5_init_context error: %s",error_message(code));
	return NIL;
    }

    krb5_init_ets(k5context);

    /* find server principal name; NULL means krb libs determine my hostname */

    code = krb5_sname_to_principal(k5context, NULL, "khttp", KRB5_NT_SRV_HST,
				   &serverp);
    if (code) {
        sprintf(reply, "Error finding server Krb5 principal name: %s",error_message(code));
	return NIL;
    }

    /* perhaps get client address?  (using getpeername)  */


    /* Check for user-specified keytab */

    if (k5_srvtab[0]) {
	code = krb5_kt_resolve(k5context, k5_srvtab, &k5keytabid);
	if (code) {
            sprintf(reply, "Error resolving keytab file: %s",error_message(code));
	    return NIL;
	}
    }

    /* and most importantly, check the client's authenticator   */

    code = krb5_rd_req(k5context, &k5auth_context, &k5authent,
		       serverp, k5keytabid, NULL, &k5ticket);
    if (code) {
	sprintf(reply, "krb5_rd_req error: %s",error_message(code));
        return NIL;
    }

    clientp = k5ticket->enc_part2->client;
   
    /* to perform further restriction through .htaccess in check_auth */

    strncpy (kdat->client_name, clientp->data->data,clientp->data->length);
    strcpy (kdat->client_realm, clientp->realm.data);
    strcpy (kdat->server_realm, serverp->realm.data);
    kdat->ver = KERBEROSV5;

    /* make sure client username matches username submitted in Auth line */

    /* removed for now; redundant and possibly buggy   ADC
    if (strncmp(kdat->client_name, user, MAX_STRING_LEN)) {
	strcpy(reply, "Permission denied; name/username mismatch."); 
        return NIL;
    }
    */
    /* send an AP_REP message to complete mutual authentication */

    code = krb5_mk_rep(k5context, k5auth_context, &k5ap_rep_data);

    if (code) {
        sprintf(reply, "krb5_mk_rep error: %s",error_message(code));
        return NIL;
    }

    /* Convert response to string and place in buffer */
    p = kdata_to_str(k5ap_rep_data.data, k5ap_rep_data.length);

    if (p) {
        *reply = '[';
        strcpy(reply+1, p);
        strcat(reply, "] User ");
        strcat(reply, user);
        strcat(reply, " authenticated");
        free(p);
    }
    else {
        /* XXX Out of memory */
        exit(1);
    }

/* call any krb5_free routines??  perhaps krb_free_ticked(k5ticket) ? */

    strncpy(user, user, MAX_STRING_LEN - 1);
    return T;
}
#endif   /* KRB5 */

int check_krb_restrict(per_request* reqInfo, security_data* sec, KerberosInfo* kdat)
{
    int grpstatus;
    char* pchGrpData = NULL;
    int ndx;
    int bValid;
    char line[MAX_STRING_LEN];
    char errstr[MAX_STRING_LEN];
    char* realm;
    char* tok;
    
    /* Common stuff: Check for valid user */
    grpstatus = 0;
    if(reqInfo->auth_grpfile) {
	if (pchGrpData = init_group(reqInfo,reqInfo->auth_grpfile))
	    grpstatus = 1;
    }

    bValid = 0;
    for(ndx=0;ndx<sec->num_auth[reqInfo->method] && !bValid;ndx++) {
        strcpy(line,sec->auth[reqInfo->method][ndx]);
	tok = strtok (line, " \t");
        if(!strcmp(tok,"valid-user")) 
	    bValid = 1;
        else if(!strcmp(tok,"user")) {
            while(tok = strtok (NULL, " \t")) {
		if (realm = strchr (tok, '@'))
		    *realm++ = '\0';

                if(!strcmp(kdat->client_name,tok)) {
		    if (!realm  && 
			!strcasecmp(kdat->server_realm, kdat->client_realm)) {
			bValid = 1;
			break;
		    }
		    else if (realm && !strcasecmp(realm, kdat->client_realm)) {
			bValid = 1;
			break;
		    }
		}
	    }
	}
	else if(!strcmp(tok,"realm")) {
            while(tok = strtok (NULL, " \t")) {
                if(!strcasecmp(kdat->client_realm,tok)) {
		    bValid = 1;
		    break;
		}
            }
        }
        else if(!strcmp(tok,"group")) {
            if(!grpstatus) {
                sprintf(errstr,"group required for %s, bad groupfile",
                        sec->d);
                auth_bong(reqInfo,errstr,reqInfo->auth_name,sec->auth_type);
            }
            while(tok = strtok (NULL, " \t")) {
                if (krb_in_group(kdat, tok, pchGrpData)) {
		    strcpy(groupname,tok);
		    bValid = 1;
		    break;
		}
            }
        }
        else
            auth_bong(reqInfo,"require not followed by user or group",
		      reqInfo->auth_name,sec->auth_type);
    }

    if(grpstatus)
	free (pchGrpData);

    return bValid;
}

int krb_in_group(KerberosInfo* kdat, char *group, char* pchGrps)
{
    char *mems, *endp = NULL;
    char *pch;
    char chSaved;
    int  nlen, bFound = 0;

    nlen = strlen (group);
    if ((mems = strstr (pchGrps, group)) && *(mems + nlen) == ':') {
	if (endp = strchr (mems + nlen + 1, ':')) {
	    while (!isspace(*endp)) endp--;
	    chSaved = *endp;
	    *endp = '\0';
	}
    }
    else
	return 0;

    nlen = strlen (kdat->client_name);
    if(pch = strstr(mems, kdat->client_name)) {
	pch += nlen;
	if (!*pch || isspace(*pch) && 
	    !strcasecmp(kdat->client_realm, kdat->server_realm)) 
	    bFound = 1;
	else if (*pch == '@') {
	    pch++;
	    nlen = strlen (kdat->client_realm);
	    if (!strncmp (kdat->client_realm, pch, nlen))
		bFound = 1;
	}
    }

    if (endp && *endp == '\0') *endp = chSaved;
    return bFound;
}

#endif   /* HAVE_KERBEROS */



