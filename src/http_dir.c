/*
 * http_dir.c: Handles the on-the-fly html index generation
 * 
 * Rob McCool
 * 3/23/93
 * 
 */


/* httpd.h includes proper directory file */
#include "httpd.h"

struct ent {
    char *name;
    char *icon;
    char *desc;
    size_t size;
    struct ent *next;
};

struct item {
    char *msg;
    char *fn;
    struct item *next;
};

static struct item *icon_list, *desc_list, *ign_list;

void init_indexing() {
    icon_list = NULL;
    desc_list = NULL;
    ign_list = NULL;
}

void kill_item_list(struct item *p) {
    struct item *q;

    while(p) {
        if(p->msg) free(p->msg);
        if(p->fn) free(p->fn);
        q = p;
        p = p->next;
        free(q);
    }
}

void kill_indexing() {
    kill_item_list(icon_list);
    kill_item_list(desc_list);
    kill_item_list(ign_list);
}

void add_icon(char *icon, char *ext, FILE *out) {
    struct item *p;

    if(!(p = (struct item *)malloc(sizeof(struct item))))
        die(NO_MEMORY,"add_icon",out);
    if(!(p->msg = strdup(icon)))
        die(NO_MEMORY,"add_icon",out);
    if(!(p->fn = strdup(ext)))
        die(NO_MEMORY,"add_icon",out);
    p->next = icon_list;
    icon_list = p;
}

char *find_icon(char *path) {
    struct item *p = icon_list;
    int pl, el;

    pl = strlen(path);
    while(p) {
        el = strlen(p->fn);
        if(el <= pl)
            if(!strcmp(&path[pl - el],p->fn))
                return p->msg;
        p = p->next;
    }
    return NULL;
}

void add_desc(char *desc, char *ext, FILE *out) {
    struct item *p;

    if(!(p = (struct item *)malloc(sizeof(struct item))))
        die(NO_MEMORY,"add_desc",out);
    if(!(p->msg = strdup(desc)))
        die(NO_MEMORY,"add_desc",out);
    if(!(p->fn = strdup(ext)))
        die(NO_MEMORY,"add_desc",out);
    p->next = desc_list;
    desc_list = p;
}

char *find_desc(char *path) {
    struct item *p = desc_list;
    int pl, el;

    pl = strlen(path);
    while(p) {
        el = strlen(p->fn);
        if(el <= pl)
            if(!strcmp(&path[pl - el],p->fn))
                return p->msg;
        p = p->next;
    }
    return NULL;
}

void add_ignore(char *ext,FILE *out) {
    struct item *p;

    if(!(p = (struct item *)malloc(sizeof(struct item))))
        die(NO_MEMORY,"add_ignore",out);
    p->msg = NULL;
    if(!(p->fn = strdup(ext)))
        die(NO_MEMORY,"add_ignore",out);
    p->next = ign_list;
    ign_list = p;
}

int ignore_entry(char *path) {
    struct item *p;
    int pl,el;

    pl = strlen(path);
    p = ign_list;
    while(p) {
        el = strlen(p->fn);
        if(el <= pl)
            if(!strcmp(&path[pl - el],p->fn))
                return 1;
        p = p->next;
    }
    return 0;
}

void insert_readme(char *name, FILE *fd) {
    char fn[MAX_STRING_LEN];
    FILE *r;
    struct stat finfo;
    int plaintext=0;

    if(!readme_fname[0])
        return;
    make_full_path(name,readme_fname,fn);
    strcat(fn,".html");
    if(stat(fn,&finfo) == -1) {
        fn[strlen(fn)-5] = '\0';
        if(stat(fn,&finfo) == -1)
            return;
        plaintext=1;
        fprintf(fd,"<HR><PRE>%c",LF);
    }
    else fprintf(fd,"<HR>");
    if(!(r = fopen(fn,"r")))
        return;
    send_fd(r,fd,"");
    fclose(r);
    if(plaintext)
        fprintf(fd,"</PRE>%c",LF);
}

struct ent *make_dir_entry(char *path, char *name, FILE *out) {
    struct ent *p;
    struct stat finfo;
    char t[MAX_STRING_LEN];

    if((name[0] == '.') && (!name[1]))
        return(NULL);

    make_full_path(path,name,t);
    if(ignore_entry(t))
        return(NULL);

    if(!(p=(struct ent *)malloc(sizeof(struct ent))))
        die(NO_MEMORY,"make_dir_entry",out);
    if(!(p->name=strdup(name)))
        die(NO_MEMORY,"make_dir_entry",out);

    if(fancy_indexing) {
        p->icon = find_icon(t);
        if((!fancy_indexing) || stat(t,&finfo) == -1) {
            p->size = -1;
        }
        else {
            if(S_ISDIR(finfo.st_mode)) {
                if(!p->icon)
                    p->icon = find_icon("**DIRECTORY**");
                p->size = -1;
            }
            else
                p->size = finfo.st_size;
        }
        p->desc = find_desc(t);
    }
    else {
        p->icon = NULL;
        p->desc = NULL;
        p->size = -1;
    }
    return(p);
}

void output_directories(struct ent **ar,int n,char *name,FILE *fd)
{
    int x;
    char blorf[MAX_STRING_LEN];

    if(name[0] == '\0') { 
        name[0] = '/'; name[1] = '\0'; 
    }
    /* aaaaargh Solaris sucks. */
    fflush(fd);
    for(x=0;x<n;x++) {
        fprintf(fd,"<%s>%c",(fancy_indexing ? "DD" : "LI"),LF);
        if(fancy_indexing && ((ar[x]->icon) || default_icon[0])) {
            fprintf(fd,"<IMG SRC=\"%s\"> ",
                    ar[x]->icon ? ar[x]->icon : default_icon);
        }
        if(strcmp(ar[x]->name,"..")) {
            make_full_path(name,ar[x]->name,blorf);
            escape_url(blorf);
            fprintf(fd,"<A NAME=\"%s\" HREF=\"%s\">%s</A>",ar[x]->name,
                    blorf,ar[x]->name);
        }
        else {
            make_full_path(name,"..",blorf);
            getparents(blorf);
            if(blorf[0] == '\0') {
                blorf[0] = '/'; blorf[1] = '\0';
            }
            escape_url(blorf);
            fprintf(fd,"<A HREF=\"%s\">%s</A>",blorf,"Parent Directory");
        }
        if(fancy_indexing) {
            if(ar[x]->desc)
                fprintf(fd," : %s",ar[x]->desc,LF);
            if(ar[x]->size != -1)
                fprintf(fd," (%d bytes)",ar[x]->size);
        }
        fputc(LF,fd);
    }
}

int dsortf(struct ent **s1,struct ent **s2)
{
    return(strcmp((*s1)->name,(*s2)->name));
}

    
void index_directory(char *name, FILE *fd)
{
    DIR *d;
    struct DIR_TYPE *dstruct;
    int num_ent=0,x;
    struct ent *head,*p,*q;
    struct ent **ar;
    char unmunged_name[MAX_STRING_LEN];

    strcpy(unmunged_name,name);
    unmunge_name(unmunged_name);

    if(!(d=opendir(name)))
        die(FORBIDDEN,unmunged_name,fd);

    strcpy(content_type,"text/html");
    if(!assbackwards) 
        send_http_header(fd);

    if(header_only) return;

/* Spew HTML preamble */
    fprintf(fd,"<HEAD><TITLE>Index of %s</TITLE></HEAD>",unmunged_name);
    fputc(LF,fd);
    fprintf(fd,"<BODY><H1>Index of %s</H1>",unmunged_name);
    fputc(LF,fd);

    fprintf(fd,fancy_indexing ? "<DL>%c" : "<UL>%c",LF);

/* 
 * Since we don't know how many dir. entries there are, put them into a 
 * linked list and then arrayificate them so qsort can use them. 
 */
    head=NULL;
    while(dstruct=readdir(d)) {
        if(p = make_dir_entry(name,dstruct->d_name,fd)) {
            p->next=head;
            head=p;
            num_ent++;
        }
    }
    if(!(ar=(struct ent **) malloc(num_ent*sizeof(struct ent *))))
        die(NO_MEMORY,"index_directory",fd);
    p=head;
    x=0;
    while(p) {
        ar[x++]=p;
        p = p->next;
    }
    
    qsort((void *)ar,num_ent,sizeof(struct ent *),
#ifdef ULTRIX_BRAIN_DEATH
          (int (*))dsortf);
#else
          (int (*)(const void *,const void *))dsortf);
#endif
     output_directories(ar,num_ent,unmunged_name,fd);
     free(ar);
     q = head;
     while(q) {
         p=q->next;
         free(q->name);
         free(q);
         q=p;
     }
     closedir(d);
     fprintf(fd,fancy_indexing ? "</DL>" : "</UL>");
     if(fancy_indexing)
         insert_readme(name,fd);
     fprintf(fd,"</BODY>");
}
