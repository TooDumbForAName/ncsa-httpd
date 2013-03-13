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
    char *alt;
    char *desc;
    size_t size;
    time_t lm;
    struct ent *next;
};


struct item {
    int type;
    char *apply_to;
    char *apply_path;
    char *data;
    struct item *next;
};


static struct item *icon_list, *alt_list, *desc_list, *ign_list;
static struct item *hdr_list, *rdme_list, *opts_list;

static int dir_opts;

void init_indexing() {
    icon_list = NULL;
    alt_list = NULL;
    desc_list = NULL;
    ign_list = NULL;

    hdr_list = NULL;
    rdme_list = NULL;
    opts_list = NULL;
}

void kill_item_list(struct item *p) {
    struct item *q;

    while(p) {
        if(p->apply_to) free(p->apply_to);
        if(p->apply_path) free(p->apply_path);
        if(p->data) free(p->data);
        q = p;
        p = p->next;
        free(q);
    }
}

void kill_indexing() {
    kill_item_list(icon_list);
    kill_item_list(alt_list);
    kill_item_list(desc_list);
    kill_item_list(ign_list);

    kill_item_list(hdr_list);
    kill_item_list(rdme_list);
    kill_item_list(opts_list);
}

struct item *new_item(int type, char *to, char *path, char *data, FILE *out) 
{
    struct item *p;

    if(!(p = (struct item *)malloc(sizeof(struct item))))
        die(NO_MEMORY,"new_item",out);

    p->type = type;
    if(data) {
        if(!(p->data = strdup(data)))
            die(NO_MEMORY,"new_item",out);
    } else
        p->data = NULL;

    if(to) {
        if(!(p->apply_to = (char *)malloc(strlen(to) + 2)))
            die(NO_MEMORY,"new_item",out);
        if((type == BY_PATH) && (!is_matchexp(to))) {
            p->apply_to[0] = '*';
            strcpy(&p->apply_to[1],to);
        } else
            strcpy(p->apply_to,to);
    } else
        p->apply_to = NULL;

    if(!(p->apply_path = (char *)malloc(strlen(path) + 2)))
        die(NO_MEMORY,"new_item",out);
    sprintf(p->apply_path,"%s*",path);

    return p;
}

void add_alt(int type, char *alt, char *to, char *path, FILE *out) {
    struct item *p;

    if(type == BY_PATH) {
        if(!strcmp(to,"**DIRECTORY**"))
            strcpy(to,"^^DIRECTORY^^");
    }
    p = new_item(type,to,path,alt,out);
    p->next = alt_list;
    alt_list = p;
}

void add_icon(int type, char *icon, char *to, char *path, FILE *out) {
    struct item *p;
    char iconbak[MAX_STRING_LEN];

    strcpy(iconbak,icon);
    if(icon[0] == '(') {
        char alt[MAX_STRING_LEN];
        getword(alt,iconbak,',');
        add_alt(type,&alt[1],to,path,out);
        iconbak[strlen(iconbak) - 1] = '\0';
    }
    if(type == BY_PATH) {
        if(!strcmp(to,"**DIRECTORY**"))
            strcpy(to,"^^DIRECTORY^^");
    }
    p = new_item(type,to,path,iconbak,out);
    p->next = icon_list;
    icon_list = p;
}

void add_desc(int type, char *desc, char *to, char *path, FILE *out) {
    struct item *p;

    p = new_item(type,to,path,desc,out);
    p->next = desc_list;
    desc_list = p;
}

void add_ignore(char *ext, char *path, FILE *out) {
    struct item *p;

    p = new_item(0,ext,path,NULL,out);
    p->next = ign_list;
    ign_list = p;
}

void add_header(char *name, char *path, FILE *out) {
    struct item *p;

    p = new_item(0,NULL,path,name,out);
    p->next = hdr_list;
    hdr_list = p;
}

void add_readme(char *name, char *path, FILE *out) {
    struct item *p;

    p = new_item(0,NULL,path,name,out);
    p->next = rdme_list;
    rdme_list = p;
}


void add_opts_int(int opts, char *path, FILE *out) {
    struct item *p;

    p = new_item(0,NULL,path,NULL,out);
    p->type = opts;
    p->next = opts_list;
    opts_list = p;
}

void add_opts(char *optstr, char *path, FILE *out) {
    char w[MAX_STRING_LEN];
    int opts = 0;

    while(optstr[0]) {
        cfg_getword(w,optstr);
        if(!strcasecmp(w,"FancyIndexing"))
            opts |= FANCY_INDEXING;
        else if(!strcasecmp(w,"IconsAreLinks"))
            opts |= ICONS_ARE_LINKS;
        else if(!strcasecmp(w,"ScanHTMLTitles"))
            opts |= SCAN_HTML_TITLES;
        else if(!strcasecmp(w,"SuppressLastModified"))
            opts |= SUPPRESS_LAST_MOD;
        else if(!strcasecmp(w,"SuppressSize"))
            opts |= SUPPRESS_SIZE;
        else if(!strcasecmp(w,"SuppressDescription"))
            opts |= SUPPRESS_DESC;
        else if(!strcasecmp(w,"None"))
            opts = 0;
    }
    add_opts_int(opts,path,out);
}


char *find_item(struct item *list, char *path, int path_only) {
    struct item *p = list;
    char *t;

    while(p) {
        /* Special cased for ^^DIRECTORY^^ and ^^BLANKICON^^ */
        if((path[0] == '^') || (!strcmp_match(path,p->apply_path))) {
            if(!(p->apply_to))
                return p->data;
            else if(p->type == BY_PATH) {
                if(!strcmp_match(path,p->apply_to))
                    return p->data;
            } else if(!path_only) {
                char pathbak[MAX_STRING_LEN];

                strcpy(pathbak,path);
                content_encoding[0] = '\0';
                set_content_type(pathbak);
                if(!content_encoding[0]) {
                    if(p->type == BY_TYPE) {
                        if(!strcmp_match(content_type,p->apply_to))
                            return p->data;
                    }
                } else {
                    if(p->type == BY_ENCODING) {
                        if(!strcmp_match(content_encoding,p->apply_to))
                            return p->data;
                    }
                }
            }
        }
        p = p->next;
    }
    return NULL;
}

#define find_icon(p,t) find_item(icon_list,p,t)
#define find_alt(p,t) find_item(alt_list,p,t)
#define find_desc(p) find_item(desc_list,p,0)
#define find_header(p) find_item(hdr_list,p,0)
#define find_readme(p) find_item(rdme_list,p,0)


int ignore_entry(char *path) {
    struct item *p = ign_list;

    while(p) {
        if(!strcmp_match(path,p->apply_path))
            if(!strcmp_match(path,p->apply_to))
                return 1;
        p = p->next;
    }
    return 0;
}

int find_opts(char *path) {
    struct item *p = opts_list;

    while(p) {
        if(!strcmp_match(path,p->apply_path))
            return p->type;
        p = p->next;
    }
    return 0;
}

int insert_readme(char *name, char *readme_fname, int rule, FILE *fd) {
    char fn[MAX_STRING_LEN];
    FILE *r;
    struct stat finfo;
    int plaintext=0;

    make_full_path(name,readme_fname,fn);
    strcat(fn,".html");
    if(stat(fn,&finfo) == -1) {
        fn[strlen(fn)-5] = '\0';
        if(stat(fn,&finfo) == -1)
            return 0;
        plaintext=1;
        if(rule) bytes_sent += fprintf(fd,"<HR>%c",LF);
        bytes_sent += fprintf(fd,"<PRE>%c",LF);
    }
    else if(rule) bytes_sent += fprintf(fd,"<HR>%c",LF);
    if(!(r = fopen(fn,"r")))
        return 0;
    send_fd(r,fd,NULL);
    fclose(r);
    if(plaintext)
        bytes_sent += fprintf(fd,"</PRE>%c",LF);
    return 1;
}


char *find_title(char *filename) {
    char titlebuf[MAX_STRING_LEN], *find = "<TITLE>";
    char filebak[MAX_STRING_LEN];
    FILE *thefile;
    int x,y,n,p;

    content_encoding[0] = '\0';
    strcpy(filebak,filename);
    set_content_type(filebak);
    if((!strcmp(content_type,"text/html")) && (!content_encoding[0])) {
        if(!(thefile = fopen(filename,"r")))
            return NULL;
        n = fread(titlebuf,sizeof(char),MAX_STRING_LEN - 1,thefile);
        titlebuf[n] = '\0';
        for(x=0,p=0;titlebuf[x];x++) {
            if(titlebuf[x] == find[p]) {
                if(!find[++p]) {
                    if((p = ind(&titlebuf[++x],'<')) != -1)
                        titlebuf[x+p] = '\0';
                    /* Scan for line breaks for Tanmoy's secretary */
                    for(y=x;titlebuf[y];y++)
                        if((titlebuf[y] == CR) || (titlebuf[y] == LF))
                            titlebuf[y] = ' ';
                    return strdup(&titlebuf[x]);
                }
            } else p=0;
        }
        return NULL;
    }
    content_encoding[0] = '\0';
    return NULL;
}


void escape_html(char *fn) {
    register int x,y;
    char copy[MAX_STRING_LEN];

    strcpy(copy,fn);
    for(x=0,y=0;copy[y];x++,y++) {
        if(copy[y] == '<') {
            strcpy(&fn[x],"&lt;");
            x+=3;
        }
        else if(copy[y] == '>') {
            strcpy(&fn[x],"&gt;");
            x+=3;
        }
        else if(copy[y] == '&') {
            strcpy(&fn[x],"&amp;");
            x+=4;
        }
        else
            fn[x] = copy[y];
    }
    fn[x] = '\0';
}

struct ent *make_dir_entry(char *path, char *name, FILE *out) {
    struct ent *p;
    struct stat finfo;
    char t[MAX_STRING_LEN], t2[MAX_STRING_LEN];

    if((name[0] == '.') && (!name[1]))
        return(NULL);

    make_full_path(path,name,t);

    if(ignore_entry(t))
        return(NULL);

    if(!(p=(struct ent *)malloc(sizeof(struct ent))))
        die(NO_MEMORY,"make_dir_entry",out);
    if(!(p->name=(char *)malloc(strlen(name) + 2)))
        die(NO_MEMORY,"make_dir_entry",out);

    if(dir_opts & FANCY_INDEXING) {
        if((!(dir_opts & FANCY_INDEXING)) || stat(t,&finfo) == -1) {
            strcpy(p->name,name);
            p->size = -1;
            p->icon = NULL;
            p->alt = NULL;
            p->desc = NULL;
            p->lm = -1;
        }
        else {
            p->lm = finfo.st_mtime;
            if(S_ISDIR(finfo.st_mode)) {
                if(!(p->icon = find_icon(t,1)))
                    p->icon = find_icon("^^DIRECTORY^^",1);
                if(!(p->alt = find_alt(t,1)))
                    p->alt = "DIR";
                p->size = -1;
                strcpy_dir(p->name,name);
            }
            else {
                p->icon = find_icon(t,0);
                p->alt = find_alt(t,0);
                p->size = finfo.st_size;
                strcpy(p->name,name);
            }
        }
        if(p->desc = find_desc(t))
            p->desc = strdup(p->desc);
        if((!p->desc) && (dir_opts & SCAN_HTML_TITLES))
            p->desc = find_title(t);
    }
    else {
        p->icon = NULL;
        p->alt = NULL;
        p->desc = NULL;
        p->size = -1;
        p->lm = -1;
        strcpy(p->name,name);
    }
    return(p);
}


void send_size(size_t size, FILE *fd) {
    char schar;

    if(size == -1) {
        fputs("    -",fd);
        bytes_sent += 5;
    }
    else {
        if(!size) {
            fputs("   0K",fd);
            bytes_sent += 5;
        }
        else if(size < 1024) {
            fputs("   1K",fd);
            bytes_sent += 5;
        }
        else if(size < 1048576)
            bytes_sent += fprintf(fd,"%4dK",size / 1024);
        else
            bytes_sent += fprintf(fd,"%4dM",size / 1048576);
    }
}

void terminate_description(char *desc) {
    int maxsize = 23;
    register int x;
    
    if(dir_opts & SUPPRESS_LAST_MOD) maxsize += 17;
    if(dir_opts & SUPPRESS_SIZE) maxsize += 7;

    for(x=0;desc[x] && maxsize;x++) {
        if(desc[x] == '<') {
            while(desc[x] != '>') {
                if(!desc[x]) {
                    maxsize = 0;
                    break;
                }
                ++x;
            }
        }
        else --maxsize;
    }
    if(!maxsize) {
        desc[x] = '>';
        desc[x+1] = '\0';
    }

}

void output_directories(struct ent **ar,int n,char *name,FILE *fd)
{
    int x,pad;
    char anchor[HUGE_STRING_LEN],t[MAX_STRING_LEN],t2[MAX_STRING_LEN];
    char *tp;

    if(name[0] == '\0') { 
        name[0] = '/'; name[1] = '\0'; 
    }
    /* aaaaargh Solaris sucks. */
    fflush(fd);

    if(dir_opts & FANCY_INDEXING) {
        fputs("<PRE>",fd);
        bytes_sent += 5;
        if(tp = find_icon("^^BLANKICON^^",1))
            bytes_sent += fprintf(fd,"<IMG SRC=\"%s\" ALT=\"     \"> ",tp);
        bytes_sent += fprintf(fd,"Name                   ");
        if(!(dir_opts & SUPPRESS_LAST_MOD))
            bytes_sent += fprintf(fd,"Last modified     ");
        if(!(dir_opts & SUPPRESS_SIZE))
            bytes_sent += fprintf(fd,"Size  ");
        if(!(dir_opts & SUPPRESS_DESC))
            bytes_sent += fprintf(fd,"Description");
        bytes_sent += fprintf(fd,"%c<HR>%c",LF,LF);
    }
    else {
        fputs("<UL>",fd);
        bytes_sent += 4;
    }
        
    for(x=0;x<n;x++) {
        if((!strcmp(ar[x]->name,"../")) || (!strcmp(ar[x]->name,".."))) {
            make_full_path(name,"..",t);
            getparents(t);
            if(t[0] == '\0') {
                t[0] = '/'; t[1] = '\0';
            }
            escape_uri(t);
            sprintf(anchor,"<A HREF=\"%s\">",t);
            strcpy(t2,"Parent Directory</A>");
        }
        else {
            strcpy(t,ar[x]->name);
            strcpy(t2,t);
            if(strlen(t2) > 21) {
                t2[21] = '>';
                t2[22] = '\0';
            }
            /* screws up formatting, but some need it. should fix. */
/*            escape_html(t2); */
            strcat(t2,"</A>");
            escape_uri(t);
            sprintf(anchor,"<A NAME=\"%s\" HREF=\"%s\">",t,t);
        }
        escape_url(t);

        if(dir_opts & FANCY_INDEXING) {
            if(dir_opts & ICONS_ARE_LINKS)
                bytes_sent += fprintf(fd,"%s",anchor);
            if((ar[x]->icon) || default_icon[0]) {
                bytes_sent += fprintf(fd,"<IMG SRC=\"%s\" ALT=\"[%s]\">",
                                      ar[x]->icon ? ar[x]->icon : default_icon,
                                      ar[x]->alt ? ar[x]->alt : "   ");
            }
            if(dir_opts & ICONS_ARE_LINKS) {
                fputs("</A>",fd);
                bytes_sent += 4;
            }
            bytes_sent += fprintf(fd," %s",anchor);
            bytes_sent += fprintf(fd,"%-27.27s",t2);
            if(!(dir_opts & SUPPRESS_LAST_MOD)) {
                if(ar[x]->lm != -1) {
                    struct tm *ts = localtime(&ar[x]->lm);
                    strftime(t,MAX_STRING_LEN,"%d-%b-%y %H:%M  ",ts);
                    fputs(t,fd);
                    bytes_sent += strlen(t);
                }
                else {
                    fputs("                 ",fd);
                    bytes_sent += 17;
                }
            }
            if(!(dir_opts & SUPPRESS_SIZE)) {
                send_size(ar[x]->size,fd);
                fputs("  ",fd);
                bytes_sent += 2;
            }
            if(!(dir_opts & SUPPRESS_DESC)) {
                if(ar[x]->desc) {
                    terminate_description(ar[x]->desc);
                    bytes_sent += fprintf(fd,"%s",ar[x]->desc);
                }
            }
        }
        else
            bytes_sent += fprintf(fd,"<LI> %s %s",anchor,t2);
        fputc(LF,fd);
        ++bytes_sent;
    }
    if(dir_opts & FANCY_INDEXING) {
        fputs("</PRE>",fd);
        bytes_sent += 6;
    }
    else {
        fputs("</UL>",fd);
        bytes_sent += 5;
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
    char *tmp;

    strncpy_dir(unmunged_name,name, MAX_STRING_LEN);
    unmunge_name(unmunged_name);

    if(!(d=opendir(name)))
        die(FORBIDDEN,unmunged_name,fd);

    strcpy(content_type,"text/html");
    status = 200;
    if(!assbackwards) 
        send_http_header(fd);

    if(header_only) return;

    bytes_sent = 0;

    dir_opts = find_opts(name);

/* Spew HTML preamble */
    bytes_sent += fprintf(fd,"<HEAD><TITLE>Index of %s</TITLE></HEAD><BODY>",
                          unmunged_name);
    fputc(LF,fd);
    ++bytes_sent;

    if((!(tmp = find_header(name))) || (!(insert_readme(name,tmp,0,fd))))
        bytes_sent += fprintf(fd,"<H1>Index of %s</H1>%c",unmunged_name,LF);

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
         if(q->desc)
             free(q->desc);
         free(q);
         q=p;
     }
     closedir(d);
     if(dir_opts & FANCY_INDEXING)
         if(tmp = find_readme(name))
             insert_readme(name,tmp,1,fd);
     else {
         fputs("</UL>",fd);
         bytes_sent += 5;
     }

     fputs("</BODY>",fd);
     bytes_sent += 7;
     log_transaction();
}
