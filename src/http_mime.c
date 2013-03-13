/*
 * http_mime.c: Sends/gets MIME headers for requests
 * 
 * Rob McCool
 * 
 */


#include "httpd.h"

struct mime_ext {
    char *ext;
    char *ct;
    struct mime_ext *next;
};

#if 0
#define hash(i) (isalpha(i) ? (tolower(i)) - 'a' : 26)
#else
#define hash(i) ((i) % 27)
#endif

/* Hash table */
struct mime_ext *types[27];
struct mime_ext *forced_types;
struct mime_ext *encoding_types;

int content_length;
char content_type[MAX_STRING_LEN];
char content_encoding[MAX_STRING_LEN];

char location[MAX_STRING_LEN];
static char last_modified[MAX_STRING_LEN];

char auth_line[MAX_STRING_LEN];

char *out_headers;
char **in_headers_env;
char *status_line;
char ims[MAX_STRING_LEN]; /* If-modified-since */


void hash_insert(struct mime_ext *me) {
    register int i = hash(me->ext[0]);
    register struct mime_ext *p, *q;

    if(!(q=types[i])) {
        types[i]=me;
        return;
    }
    if((!(p=q->next)) && (strcmp(q->ext,me->ext) >= 0)) {
        types[i]=me;
        me->next=q;
        return;
    }
    while(p) {
        if(strcmp(p->ext,me->ext) >= 0) break;
        q=p;
        p=p->next;
    }
    me->next=p;
    q->next=me;
}

void kill_mime() {
    register struct mime_ext *p,*q;
    register int x;

    for(x=0;x<27;x++) {
        p=types[x];
        while(p) {
            free(p->ext);
            free(p->ct);
            q=p;
            p=p->next;
            free(q);
        }
    }
    p=forced_types;
    while(p) {
        free(p->ext);
        free(p->ct);
        q=p;
        p=p->next;
        free(q);
    }
    p=encoding_types;
    while(p) {
        free(p->ext);
        free(p->ct);
        q=p;
        p=p->next;
        free(q);
    }
}

void init_mime() {
    char l[MAX_STRING_LEN],w[MAX_STRING_LEN],*ct;
    FILE *f;
    register struct mime_ext *me;
    register int x,y;

    if(!(f = fopen(types_confname,"r"))) {
        fprintf(stderr,"httpd: could not open mime types file %s\n",
                types_confname);
        perror("fopen");
        exit(1);
    }

    for(x=0;x<27;x++) 
        types[x] = NULL;
    forced_types = NULL;
    encoding_types = NULL;

    while(!(cfg_getline(l,MAX_STRING_LEN,f))) {
        if(l[0] == '#') continue;
        cfg_getword(w,l);
        if(!(ct = (char *)malloc(sizeof(char) * (strlen(w) + 1))))
            die(NO_MEMORY,"init_mime",stderr);
        strcpy(ct,w);

        while(l[0]) {
            cfg_getword(w,l);
            if(!(me = (struct mime_ext *)malloc(sizeof(struct mime_ext))))
                die(NO_MEMORY,"init_mime",stderr);
            if(!(me->ext = (char *)malloc(sizeof(char) * (strlen(w)+1))))
                die(NO_MEMORY,"init_mime",stderr);
            for(x=0;w[x];x++)
                me->ext[x] = (islower(w[x]) ? w[x] : tolower(w[x]));
            me->ext[x] = '\0';
            if(!(me->ct=strdup(ct)))
                die(NO_MEMORY,"init_mime",stderr);
            me->next=NULL;
            hash_insert(me);
        }
        free(ct);
    }
    fclose(f);
}

void dump_types() {
    struct mime_ext *p;
    register int x;

    for(x=0;x<27;x++) {
        p=types[x];
        while(p) {
            printf("ext %s: %s\n",p->ext,p->ct);
            p=p->next;
        }
    }
    p=forced_types;
    while(p) {
        printf("file %s: %s\n",p->ext,p->ct);
        p=p->next;
    }
}

int is_content_type(char *type) {
    return(!strcmp(content_type,type));
}

void find_ct(char *file, int store_encoding) {
    int i,l,l2;
    struct mime_ext *p;
    char fn[MAX_STRING_LEN];

    strcpy(fn,file);
    if((i=rind(fn,'.')) >= 0) {
        ++i;
        l=strlen(fn);
        p = encoding_types;

        while(p) {
            if(!strcmp(p->ext,&fn[i])) {
                fn[i-1] = '\0';
                if(store_encoding) {
                    if(content_encoding[0])
                        sprintf(content_encoding,"%s, %s",content_encoding,
                                p->ct);
                    else
                        strcpy(content_encoding,p->ct);
                }
                if((i=rind(fn,'.')) < 0)
                    break;
                ++i;
                l=strlen(fn);
                p=encoding_types;
            }
            else
                p=p->next;
        }
    }
    p=forced_types;
    l=strlen(fn);

    while(p) {
        l2=l-strlen(p->ext);
        if((l2 >= 0) && (!strcasecmp(p->ext,&fn[l2]))) {
            strcpy(content_type,p->ct);
            return;
        }
        p=p->next;
    }

    if((i = rind(fn,'.')) < 0) {
        strcpy(content_type,default_type);
        return;
    }
    ++i;
    p=types[hash(fn[i])];

    while(p) {
        if(!strcasecmp(p->ext,&fn[i])) {
            strcpy(content_type,p->ct);
            return;
        }
        p=p->next;
    }
    strcpy(content_type,default_type);
}



void probe_content_type(char *file) {
    find_ct(file,0);
}

void set_content_type(char *file) {
    find_ct(file,1);
}

int scan_script_header(FILE *f, FILE *fd) {
    char w[MAX_STRING_LEN];
    char *l;
    int p;

    while(1) {
        if(getline(w,MAX_STRING_LEN-1,fileno(f),timeout))
            die(SERVER_ERROR,"httpd: malformed header from script",fd);

        if(w[0] == '\0') return is_url(location);
        if(!(l = strchr(w,':')))
            l = w;
        *l++ = '\0';
        if(!strcasecmp(w,"Content-type"))
            sscanf(l,"%s",content_type);
        else if(!strcasecmp(w,"Location"))
            sscanf(l,"%s",location);
        else if(!strcasecmp(w,"Status")) {
            for(p=0;isspace(l[p]);p++);
            sscanf(&l[p],"%d",&status);
            if(!(status_line = strdup(&l[p])))
                die(NO_MEMORY,"scan_script_header",fd);
        }
        else {
            *(--l) = ':';
            for(p=0;w[p];p++);
            w[p] = LF;
            w[++p] = '\0';
            if(!out_headers) {
                if(!(out_headers = strdup(w)))
                    die(NO_MEMORY,"scan_script_header",fd);
            }
            else {
                int loh = strlen(out_headers);
                out_headers = (char *) realloc(out_headers,
                                               (loh+strlen(w)+1)*sizeof(char));
                if(!out_headers)
                    die(NO_MEMORY,"scan_script_header",fd);
                strcpy(&out_headers[loh],w);
            }
        }
    }
}

void add_type(char *fn, char *t, FILE *out) {
    struct mime_ext *n;

    if(!(n=(struct mime_ext *)malloc(sizeof(struct mime_ext))))
        die(NO_MEMORY,"add_type",out);

    if(!(n->ext = strdup(fn)))
        die(NO_MEMORY,"add_type",out);
    if(!(n->ct = strdup(t)))
        die(NO_MEMORY,"add_type",out);
    n->next = forced_types;
    forced_types = n;
}

void add_encoding(char *fn, char *t,FILE *out) {
    struct mime_ext *n;

    if(!(n=(struct mime_ext *)malloc(sizeof(struct mime_ext))))
        die(NO_MEMORY,"add_encoding",out);

    if(!(n->ext = strdup(fn)))
        die(NO_MEMORY,"add_encoding",out);
    if(!(n->ct = strdup(t)))
        die(NO_MEMORY,"add_encoding",out);
    n->next = encoding_types;
    encoding_types = n;
}

void set_content_length(int l) {
    content_length = l;
}

void set_last_modified(time_t t, FILE *out) {
    strcpy(last_modified,gm_timestr_822(t));

    if(!ims[0])
        return;

    if(later_than(last_modified, ims))
        die(USE_LOCAL_COPY,NULL,out);
}

void init_header_vars() {
    content_type[0] = '\0';
    last_modified[0] = '\0';
    content_length = -1;
    auth_line[0] = '\0';
    content_encoding[0] = '\0';
    location[0] = '\0';
    ims[0] = '\0';
    status_line = NULL;
    out_headers = NULL;
    in_headers_env = NULL;
}

int merge_header(char *h, char *v, FILE *out) {
    register int x,l,lt;
    char **t;

    for(l=0;h[l];++l);
    h[l] = '=';
    h[++l] = '\0';

    for(t=in_headers_env;*t;++t) {
        if(!strncmp(*t,h,l)) {
            lt = strlen(*t);
            if(!(*t = (char *) realloc(*t,(lt+strlen(v)+3)*sizeof(char))))
                die(NO_MEMORY,"merge_header",out);
            (*t)[lt++] = ',';
            (*t)[lt++] = ' ';
            strcpy(&((*t)[lt]),v);
            return 1;
        }
    }
    h[l-1] = '\0';
    return 0;
}

void get_mime_headers(int fd, FILE *out) {
    char w[MAX_STRING_LEN];
    char l[MAX_STRING_LEN];
    int num_inh, num_processed;
    char *t;

    num_inh = 0;
    num_processed = 0;

    while(!(getline(w,MAX_STRING_LEN-1,fd,timeout))) {
        if(!w[0]) 
            return;
        if((++num_processed) > MAX_HEADERS)
            die(BAD_REQUEST,"too many header lines",out);
        if(!(t = strchr(w,':')))
            continue;
        *t++ = '\0';
        while(isspace(*t)) ++t;
        strcpy(l,t);

        if(!strcasecmp(w,"Content-type")) {
            strcpy(content_type,l);
            continue;
        }
        if(!strcasecmp(w,"Authorization")) {
            strcpy(auth_line,l);
            continue;
        }
        if(!strcasecmp(w,"Content-length")) {
            sscanf(l,"%d",&content_length);
            continue;
        }
        if(!strcasecmp(w,"If-modified-since"))
            strcpy(ims,l);

        http2cgi(w);
        if(in_headers_env) {
            if(!merge_header(w,l,out)) {
                in_headers_env = 
                    (char **) realloc(in_headers_env,
                                      (num_inh+2)*sizeof(char *));
                if(!in_headers_env)
                    die(NO_MEMORY,"get_mime_headers",out);
                in_headers_env[num_inh++] = make_env_str(w,l,out);
                in_headers_env[num_inh] = NULL;
            }
        }
        else {
            if(!(in_headers_env = (char **) malloc(2*sizeof(char *))))
                die(NO_MEMORY,"get_mime_headers",out);
            in_headers_env[num_inh++] = make_env_str(w,l,out);
            in_headers_env[num_inh] = NULL;
        }
    }
}


void dump_default_header(FILE *fd) {
    fprintf(fd,"Date: %s%c",gm_timestr_822(time(NULL)),LF);
    fprintf(fd,"Server: %s%c",SERVER_VERSION,LF);
    fprintf(fd,"MIME-version: 1.0%c",LF);
}

void send_http_header(FILE *fd) {
    if(!status_line) {
        if(location[0]) {
            status = 302;
            status_line = "302 Found";
        }
        else {
            status = 200;
            status_line = "200 OK";
        }
    }            
    begin_http_header(fd,status_line);
    if(content_type[0])
        fprintf(fd,"Content-type: %s%c",content_type,LF);
    if(last_modified[0])
        fprintf(fd,"Last-modified: %s%c",last_modified,LF);
    if(content_length >= 0) 
        fprintf(fd,"Content-length: %d%c",content_length,LF);
    if(location[0])
        fprintf(fd,"Location: %s%c",location,LF);
    if(content_encoding[0])
        fprintf(fd,"Content-encoding: %s%c",content_encoding,LF);
    if(out_headers)
        fprintf(fd,"%s",out_headers);
    fprintf(fd,"%c",LF);
}
