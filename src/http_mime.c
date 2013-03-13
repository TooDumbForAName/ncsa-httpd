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

#define hash(i) (isalpha(i) ? (tolower(i)) - 'a' : 26)

/* Hash table */
struct mime_ext *types[27];
struct mime_ext *forced_types;
struct mime_ext *encoding_types;

int content_length;
char content_type[MAX_STRING_LEN];

char location[MAX_STRING_LEN];
static char last_modified[MAX_STRING_LEN];
static char content_encoding[MAX_STRING_LEN];
char http_accept[HUGE_STRING_LEN];

char auth_line[MAX_STRING_LEN];

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

void set_content_type(char *file) {
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
                if(content_encoding[0])
                    sprintf(content_encoding,"%s, %s",content_encoding,p->ct);
                else
                    strcpy(content_encoding,p->ct);
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

int scan_script_header(FILE *f, FILE *fd) {
    char l[MAX_STRING_LEN];
    char w[MAX_STRING_LEN];

    while(1) {
        if(getline(l,MAX_STRING_LEN,fileno(f),timeout))
            die(SERVER_ERROR,"httpd: malformed header from script",fd);

        if(l[0] == '\0') return is_url(location);
        getword(w,l,':');
        if(!strcasecmp(w,"Content-type"))
            sscanf(l,"%s",content_type);
        else if(!strcasecmp(w,"Location"))
            sscanf(l,"%s",location);
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

void set_last_modified(time_t t) {
    strcpy(last_modified,gm_timestr_822(t));
}

void get_mime_headers(int fd) {
    char l[MAX_STRING_LEN];
    char w[MAX_STRING_LEN];

    content_type[0] = '\0';
    last_modified[0] = '\0';
    content_length = -1;
    auth_line[0] = '\0';
    content_encoding[0] = '\0';
    http_accept[0] = '\0';
    location[0] = '\0';

    while(!(getline(l,MAX_STRING_LEN,fd,timeout))) {
        getword(w,l,':');
        if(!strcasecmp(w,"Content-type")) 
            sscanf(l,"%s",content_type);
        else if(!strcasecmp(w,"Authorization"))
            strcpy(auth_line,l);
        else if(!strcasecmp(w,"Content-length"))
            sscanf(l,"%d",&content_length);
        else if(!strcasecmp(w,"Accept")) {
            if(http_accept[0])
                sprintf(http_accept,"%s, %s%c",http_accept,l,'\0');
            else
                strcpy(http_accept,l);
        }
        else if(!l[0])
            return;
    }
}

void dump_default_header(FILE *fd) {
    fprintf(fd,"Date: %s%c",gm_timestr_822(time(NULL)),LF);
    fprintf(fd,"Server: %s%c",SERVER_VERSION,LF);
    fprintf(fd,"MIME-version: 1.0%c",LF);
}

void send_http_header(FILE *fd) {
    begin_http_header(fd,(location[0] ? "302 Found" : "200 OK"));
    if(content_type[0])
        fprintf(fd,"Content-type: %s%c",content_type,LF);
    if(last_modified[0])
        fprintf(fd,"Last-modified: %s%c",last_modified,LF);
    if(content_length >= 0) 
        fprintf(fd,"Content-length: %d%c",content_length,LF);
    if(location[0])
        fprintf(fd,"Location: %s%c",location,LF);
    if(content_encoding[0])
        fprintf(fd,"Content-encoding: %s%c",content_encoding,LF,LF);

    fprintf(fd,"%c",LF);
}
