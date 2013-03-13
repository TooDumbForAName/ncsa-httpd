/*
 * http_auth: authentication
 * 
 * All code contained herein is covered by the Copyright as distributed
 * in the README file in the main directory of the distribution of 
 * NCSA HTTPD.
 *
 * Based on NCSA HTTPd 1.3 by Rob McCool
 * 
 */


#include "httpd.h"

char user[MAX_STRING_LEN];
char groupname[MAX_STRING_LEN];


void auth_bong(char *s, FILE *out) {
    char errstr[MAX_STRING_LEN];

/* debugging */
    if(s) {
        sprintf(errstr,"%s authorization: %s",remote_name,s);
        log_error(errstr);
    }
    if(!strcasecmp(auth_type,"Basic")) {
        sprintf(errstr,"Basic realm=\"%s\"",auth_name);
        die(AUTH_REQUIRED,errstr,out);
    }
    else {
        sprintf(errstr,"Unknown authorization method %s",auth_type);
        die(SERVER_ERROR,errstr,out);
    }
}

void check_auth(security_data *sec, int m, FILE *out) {
    char at[MAX_STRING_LEN];
    char ad[MAX_STRING_LEN];
    char sent_pw[MAX_STRING_LEN];
    char real_pw[MAX_STRING_LEN];
    char t[MAX_STRING_LEN];
    char w[MAX_STRING_LEN];
    char errstr[MAX_STRING_LEN];
    register int x,y;
    int grpstatus;

    if(!auth_type) {
        sprintf(errstr,
"httpd: authorization required for %s but not configured",sec->d);
        die(SERVER_ERROR,errstr,out);
    }

    if(!strcasecmp(auth_type,"Basic")) {
        if(!auth_name) {
            sprintf(errstr,"httpd: need AuthName for %s",sec->d);
            die(SERVER_ERROR,errstr,out);
        }
        if(!auth_line[0])
            auth_bong(NULL,out);
        if(!auth_pwfile) {
            sprintf(errstr,"httpd: need AuthUserFile for %s",sec->d);
            die(SERVER_ERROR,errstr,out);
        }
        sscanf(auth_line,"%s %s",at,t);
        if(strcmp(at,auth_type))
            auth_bong("type mismatch",out);
        uudecode(t,(unsigned char *)ad,MAX_STRING_LEN);
        getword(user,ad,':');
        strcpy(sent_pw,ad);
        if(!get_pw(user,real_pw,out)) {
            sprintf(errstr,"user %s not found",user);
            auth_bong(errstr,out);
        }
        /* anyone know where the prototype for crypt is? */
        if(strcmp(real_pw,(char *)crypt(sent_pw,real_pw))) {
            sprintf(errstr,"user %s: password mismatch",user);
            auth_bong(errstr,out);
        }
    }
    else {
        sprintf(errstr,"unknown authorization type %s for %s",auth_type,
                sec->d);
        auth_bong(errstr,out);
    }

    /* Common stuff: Check for valid user */
    if(auth_grpfile)
        grpstatus = init_group(auth_grpfile,out);
    else
        grpstatus = 0;

    for(x=0;x<sec->num_auth[m];x++) {
        strcpy(t,sec->auth[m][x]);
        getword(w,t,' ');
        if(!strcmp(w,"valid-user"))
            goto found;
        if(!strcmp(w,"user")) {
            while(t[0]) {
                if(t[0] == '\"') {
                    getword(w,&t[1],'\"');
                    for(y=0;t[y];y++)
                        t[y] = t[y+1];
                }
                getword(w,t,' ');
                if(!strcmp(user,w))
                    goto found;
            }
        }
        else if(!strcmp(w,"group")) {
            if(!grpstatus) {
                sprintf(errstr,"group required for %s, bad groupfile",
                        sec->d);
                auth_bong(errstr,out);
            }
            while(t[0]) {
                getword(w,t,' ');
                if(in_group(user,w)) {
		    strcpy(groupname,w);
                    goto found;
		}
            }
        }
        else
            auth_bong("require not followed by user or group",out);
    }
    if(grpstatus) kill_group();
    sprintf(errstr,"user %s denied",user);
    auth_bong(errstr,out);
  found:
    if(grpstatus)
        kill_group();
}

