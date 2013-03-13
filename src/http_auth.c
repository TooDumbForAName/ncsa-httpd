/*
 * http_auth: authentication
 * 
 * Rob McCool
 * 
 */


#include "httpd.h"

char user[MAX_STRING_LEN];

#ifdef PEM_AUTH
int doing_pem;
static char pem_user[MAX_STRING_LEN];
#define ENCODING_NONE -1
#define ENCODING_PEM 1
#define ENCODING_PGP 2
#endif

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
#ifdef PEM_AUTH
    if(!strcasecmp(auth_type,"PEM")) {
        sprintf(errstr,"PEM entity=\"%s\"",
                auth_pem_entity);
        die(AUTH_REQUIRED,errstr,out);
    }
    if(!strcasecmp(auth_type,"PGP")) {
        sprintf(errstr,"PGP entity=\"%s\"",auth_pgp_entity);
        die(AUTH_REQUIRED,errstr,out);
    }
#endif
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
    register int x;
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
        if(!get_pw(user,real_pw)) {
            sprintf(errstr,"user %s not found",user);
            auth_bong(errstr,out);
        }
        /* anyone know where the prototype for crypt is? */
        if(strcmp(real_pw,(char *)crypt(sent_pw,real_pw))) {
            sprintf(errstr,"user %s: password mismatch",user);
            auth_bong(errstr,out);
        }
    }
#ifdef PEM_AUTH
    else if(!strcasecmp(auth_type,"PEM")) {
        /* see if we're already handling the request... */
        switch(doing_pem) {
          case ENCODING_NONE:
            auth_bong(NULL,out);
          case ENCODING_PGP:
            auth_bong("request with pgp for pem-protected directory",out);
          default:
            strcpy(user,pem_user);
        }
    }
    else if(!strcasecmp(auth_type,"PGP")) {
        switch(doing_pem) {
          case ENCODING_NONE:
            auth_bong(NULL,out);
          case ENCODING_PEM:
            auth_bong("request with pem for pgp-protected directory",out);
          default:
            strcpy(user,pem_user);
        }
        strcpy(user,pem_user);
    }
#endif
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
                if(in_group(user,w))
                    goto found;
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

#ifdef PEM_AUTH

static int encrypt_pid,decrypt_pid;
static char tn[L_tmpnam];

void pem_cleanup(int status, FILE *out) {
    if(doing_pem != ENCODING_NONE) {
        fclose(out);
        waitpid(decrypt_pid,NULL,0);
        waitpid(encrypt_pid,NULL,0);
        unlink(tn);
    }
}

int pem_decrypt(int sfd, char *req, FILE **out) {
    int tfd,nr,pid,p[2],decrypt_fd,pem,pgp;
    char w[MAX_STRING_LEN],w2[MAX_STRING_LEN];
    char c;
    FILE *tmp;
    char *decrypt,*encrypt,*entity;

    doing_pem = ENCODING_NONE;
    if(strcmp(req,"/"))
        return -1;

    pem = !(strcmp(content_type,"application/x-www-pem-request"));
    pgp = !(strcmp(content_type,"application/x-www-pgp-request"));
    if((!pem) && (!pgp))
        return -1;

    auth_type = (pem ? "PEM" : "PGP");

    sscanf(auth_line,"%s %s",w,w2);

    if(pem) {
        if(strcasecmp(w,"PEM"))
            auth_bong("PEM content in reply to non-PEM request",*out);
    }
    else {
        if(strcasecmp(w,"PGP"))
            auth_bong("PGP content in reply to non-PGP request",*out);
    }

    getword(w,w2,'=');
    if(strcasecmp(w,"entity"))
        auth_bong("Garbled entity line",*out);

    auth_type = NULL; /* prevent its being freed later */
    if(w2[0] == '\"')
        getword(pem_user,&w2[1],'\"');
    else
        strcpy(pem_user,w2);

    escape_shell_cmd(pem_user); /* protect from security threats */
    if(pem) {
        decrypt = auth_pem_decrypt;
        encrypt = auth_pem_encrypt;
        entity = auth_pem_entity;
    }
    else {
        decrypt = auth_pgp_decrypt;
        encrypt = auth_pgp_encrypt;
        entity = auth_pgp_entity;
    }

    if((!encrypt[0]) || (!decrypt[0]) || (!entity[0]))
        die(SERVER_ERROR,"PEM/PGP authorization required but not configured",
            *out);

    tmpnam(tn);
    if((tfd = open(tn,O_WRONLY|O_CREAT,S_IRUSR|S_IWUSR)) == -1)
        die(SERVER_ERROR,"Could not open a temp file for writing",*out);

    nr=0;
    while(nr != content_length) {
        char buf[1024];
        int n,t;
        t = content_length-nr;
        if((n = read(sfd,buf,(t > 1024 ? 1024 : t))) < 1) {
            close(tfd);
            unlink(tn);
            die(SERVER_ERROR,"Failed to read the full encrypted request",*out);
        }
        if(write(tfd,buf,n) != n) {
            close(tfd);
            unlink(tn);
            die(SERVER_ERROR,"Could not write to temp file",*out);
        }
        nr+=n;
    }
    close(tfd);
    /* this is done here instead of below for error recovery */
    if((tfd = open(tn,O_RDONLY)) == -1) {
        unlink(tn);
        die(SERVER_ERROR,"Could not open temp file for reading",*out);
    }

    if(pipe(p) == -1)
        die(SERVER_ERROR,"Could not create IPC pipe",*out);
    if((pid = fork()) == -1)
        die(SERVER_ERROR,"Could not fork a new process",*out);

    if(!pid) {
        char *argv0;
        close(p[0]);
        signal(SIGHUP,SIG_DFL);
        if(tfd != STDIN_FILENO) {
            dup2(tfd,STDIN_FILENO);
            close(tfd);
        }
        if(p[1] != STDOUT_FILENO) {
            dup2(p[1],STDOUT_FILENO);
            close(p[1]);
        }
        if(!(argv0 = strrchr(decrypt,'/')))
            argv0 = decrypt;
        else
            ++argv0;
        if(execlp(decrypt,argv0,pem_user,(char *)0) == -1)
            die(SERVER_ERROR,"Could not exec decrypt command",*out); /* !!! */
    }
    close(tfd);
    close(p[1]);
    
    decrypt_fd = p[0];
    decrypt_pid = pid;

    /* create encryption stream */
    if(pipe(p) == -1)
        die(SERVER_ERROR,"Could not open an IPC pipe",*out);
    if((pid = fork()) == -1)
        die(SERVER_ERROR,"Could not fork new process",*out);

    if(!pid) {
        char *argv0;
        signal(SIGHUP,SIG_DFL);
        close(p[1]);
        if(fileno(*out) != STDOUT_FILENO) {
            dup2(fileno(*out),STDOUT_FILENO);
            fclose(*out);
        }
        if(p[0] != STDIN_FILENO) {
            dup2(p[0],STDIN_FILENO);
            close(p[0]);
        }
        if(!(argv0 = strrchr(encrypt,'/')))
            argv0 = encrypt;
        else
            ++argv0;
        if(execlp(encrypt,argv0,pem_user,(char*)0) == -1)
            die(SERVER_ERROR,"Could not exec encrypt command",*out); /*!!!*/
    }
    close(p[0]);
    tmp = *out;
    if(!(*out = fdopen(p[1],"w")))
        die(SERVER_ERROR,"Could not open stream to encrypt command",tmp);
    strcpy(content_type,(pem ? "application/x-www-pem-reply" : 
                         "application/x-www-pgp-reply"));
    doing_pem = (pem ? ENCODING_PEM : ENCODING_PGP);
    location[0] = '\0';
    set_content_length(-1);
    send_http_header(tmp);
    fclose(tmp);

    encrypt_pid = pid;
    return(decrypt_fd);
}

#endif
