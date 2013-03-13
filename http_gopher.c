/*
 * http_gopher.c: groks gopher directories and spits back HTML links
 * 
 * This is big and ugly.
 * 
 * Rob McCool (robm@ncsa.uiuc.edu)
 */

#include "httpd.h"


#ifdef GROK_GOPHER
/* Great green globs of greasy grimy gopher guts... */


typedef struct glist {
    char type;
    char name[MAX_STRING_LEN];
    int number;
    char host[MAX_STRING_LEN];
    int port;
    char path[MAX_STRING_LEN]; 
    struct glist *next;
} gopher_ent;

/* Number is optional */
#define PATH 1
#define TYPE (1<<1)
#define NAME (1<<2)
#define PORT (1<<3)
#define HOST (1<<4)
#define ALL (PATH | TYPE | NAME | PORT | HOST)

#define NUM_IGNORE 9

static char *ignore_files[]=
{
    ".",
    "..",
    "bin",
    "core",
    "dev",
    "etc",
    "home",
    "usr",
    "lost+found"
    };

static FILE *fd;

int ignore(char *name) {
    int x;

    for(x=0;x<NUM_IGNORE;x++)
        if(!strcmp(ignore_files[x],name))
            return 1;

    return 0;
}

void process_cap_file(char *name,gopher_ent **ary,int n) {
    char path[MAX_STRING_LEN];
    DIR *d;FILE *f;
    struct DIR_TYPE *cap;
    int l,i;

    make_full_path(name,".cap",path);
    l=strlen(path);
    if(!(d=opendir(path)))
        return;

    path[l++]='/';
    while(cap=readdir(d)) {
        strcpy(&path[l],cap->d_name);
        if(!is_directory(path)) {
            if(f=fopen(path,"r")) {
                while(!feof(f)) {
                    char line[MAX_STRING_LEN];
                    fgets(line,MAX_STRING_LEN,f);
                    if(!strncmp(line,"Name=",5)) {
                        for(i=0;i<n;i++) {
                            if(!strcmp(ary[i]->path,cap->d_name)) {
                                strcpy_nocrlf(ary[i]->name,&line[5]);
                                break;
                            }
                        }
                    }
                    if(!strncmp(line,"Numb=",5)) {
                        for(i=0;i<n;i++) {
                            if(!strcmp(ary[i]->path,cap->d_name)) {
                                ary[i]->number=atoi(&line[5]);
                                break;
                            }
                        }
                    }
                }
                fclose(f);
            }
        }
    }
    closedir(d);
}

int gsortf(gopher_ent **g1,gopher_ent **g2) {
    register int i1=(*g1)->number,i2=(*g2)->number;

    if((i1 >= 0) && (i2 >= 0)) {
        if(i1 == i2) return 0;
        else if(i1 > i2) return 1;
        else return -1;
    }
    else if(i1 >= 0) return -1;
    else if(i2 >= 0) return 1;

    else return(strcmp((*g1)->name,(*g2)->name));
}

gopher_ent **sort_list(gopher_ent *head,char *name,int n,int cap_file) {
    gopher_ent **ret;
    gopher_ent *p=head->next;
    int i=0;

    if(!(ret=(gopher_ent **)malloc(n*sizeof(gopher_ent *))))
        server_error(fd,MEMORY);

    while(p) {
        ret[i++]=p;
        p=p->next;
    }
    if(cap_file) process_cap_file(name,ret,n);
    qsort((void *)ret,n,sizeof(gopher_ent *),
#ifdef ULTRIX_BRAIN_DEATH
          (int (*))gsortf);
#else
          (int (*)(const void *,const void *))gsortf);
#endif
    return ret;
}

void spew_and_free_array(char *name,gopher_ent **ar,int n) {
    int lnum=0,i;

    if(name[0]=='/' && !(name[1])) name[0]='\0';


    for(i=0;i<n;i++) {
        switch(ar[i]->type) {
          case '0':
          case '1':
          case '4':
          case '5':
          case '6':
          case '9':
          case 'g':
          case 'I':
          case 'h':
          case 's':
            /* THIS NEEDS WORK... */
            if(!strncmp(ar[i]->path,"ftp:",4)) {
                int y=4;

                fprintf(fd,"<LI> <A NAME=%d HREF=\"file://",lnum++);
                while(ar[i]->path[y]!='@' && ar[i]->path[y]) 
                    fputc(ar[i]->path[y++],fd);
                if(ar[i]->path[y])
                    while(ar[i]->path[++y])
                        fputc(ar[i]->path[y],fd);
                else
                    fputc('/',fd);
                fprintf(fd,"\">%s</A>%c",ar[i]->name,LINEFEED);
                break;
            }
            if(ar[i]->host[0]) {
                char *host;

                host = full_hostname();
                /* problem: if we fall through here we may get // */
                if(strcmp(host,ar[i]->host)) {
                    if(ar[i]->path[0]) {
                        fprintf(fd,
                                "<LI> <A NAME=%d HREF=\"gopher://%s:%d/%c%s\"",
                                lnum++,ar[i]->host,ar[i]->port,ar[i]->path[0],
                                ar[i]->path);
                    }
                    else {
                        fprintf(fd,"<LI> <A NAME=%d HREF=\"gopher://%s:%d/\"",
                                lnum++,ar[i]->host,ar[i]->port);
                    }
                    fprintf(fd,">%s</A>%c",ar[i]->name,LINEFEED);
                    break;
                }
            }
            /* check the path to see if it starts with 0/ or 1/ */
            if((ar[i]->path[0] == '0') && (ar[i]->path[1] == '/'))
                strsubfirst(2,ar[i]->path,"");
            else if((ar[i]->path[0] == '1') && (ar[i]->path[1] == '/'))
                strsubfirst(2,ar[i]->path,"");
            fprintf(fd,"<LI> <A NAME=%d HREF=\"%s/%s\">%s</A>%c",
                    lnum++,name,ar[i]->path,ar[i]->name,LINEFEED);
            break;
          case '8':
            fprintf(fd,"<LI> <A NAME=%d HREF=\"telnet://%s:%d/\">%s</A>%c",
                    lnum++,ar[i]->host,ar[i]->port,ar[i]->name,LINEFEED);
            break;
          case 'T':
            fprintf(fd,"<LI> <A NAME=%d HREF=\"tn3270://%s:%d/\">%s</A>%c",
                    lnum++,ar[i]->host,ar[i]->port,ar[i]->name,LINEFEED);
            break;
        }
        free(ar[i]);
    }
    free(ar);
}

gopher_ent *add_local_link(char *name, gopher_ent *head) {
    gopher_ent *p;
    
    if(!(p=(gopher_ent *)malloc(sizeof(gopher_ent))))
        server_error(fd,MEMORY);
    p->type='0';
    p->number=-1;
    strcpy(p->path,name);
    strcpy(p->name,name);
    p->host[0]='\0';
    p->port=-1;
    p->next=head->next;
    head->next=p;
}

int add_links_from_file(char *dir,char *name,gopher_ent *head) {
    char tstr[MAX_STRING_LEN];
    FILE *f;
    gopher_ent *p;
    int n=0;

    make_full_path(dir,name,tstr);
    /* Gopher is stupid. */
    if(!(f=fopen(tstr,"r"))) {
        return;
    }

    while(!feof(f)) {
        int g;

        if(!(p=(gopher_ent *)malloc(sizeof(gopher_ent))))
            server_error(fd,MEMORY);
        p->number=-1;
        g=0;
        while(g!=ALL) {
            if(feof(f)) {
                free(p);
                p=NULL;
                break;
            }
            fgets(tstr,MAX_STRING_LEN,f);
            if(!strncmp(tstr,"Name=",5)) {
                strcpy_nocrlf(p->name,&tstr[5]);
                g|=NAME;
            }
            else if(!strncmp(tstr,"Host=",5)) {
                strcpy_nocrlf(p->host,&tstr[5]);
                g|=HOST;
            }
            else if(!strncmp(tstr,"Path=",5)) {
                strcpy_nocrlf(p->path,&tstr[5]);
                g|=PATH;
            }
            else if(!strncmp(tstr,"Port=",5)) {
                if(tstr[5])
                    sscanf(&tstr[5],"%d",&p->port);
                else p->port=-1;
                g|=PORT;
            }
            else if(!strncmp(tstr,"Type=",5)) {
                p->type=tstr[5];
                g|=TYPE;
            }
            else if(!strncmp(tstr,"Numb=",5)) {
                if(tstr[5])
                    sscanf(&tstr[5],"%d",&p->number);
            }
        }
        if(p) {
            p->next=head->next;
            head->next=p;
            n++;
        }
    }
    return(n);
}

void gopher_index(char *name, FILE *outfile) {
    DIR *d;
    struct DIR_TYPE *dstruct;
    gopher_ent head;
    gopher_ent **sorted;
    int num_entries;
    char unmunged_name[MAX_STRING_LEN];
    int cap_file=0;

    fd=outfile;
    head.next=NULL;

    strcpy(unmunged_name,name);
    unmunge_name(unmunged_name);

    if(!(d=opendir(name)))
        client_error(fd,BAD_FILE);

    while(dstruct=readdir(d)) {
        char *fn;

        fn=dstruct->d_name;
        if(ignore(fn))
            continue;

        if(fn[0]=='.') {
            if(!strcmp(fn,".cap"))
                cap_file=1;
            else
                num_entries+=add_links_from_file(name,fn,&head);
        }
        else {
            add_local_link(fn,&head);
            num_entries++;
        }
    }
    fprintf(fd,"<TITLE> Gopher Index of %s</TITLE>%c",unmunged_name,LINEFEED);
    fprintf(fd,"<H1>Gopher Index of %s</H1>%c",unmunged_name,LINEFEED);
    fprintf(fd,"<UL>%c",LINEFEED);

    if(num_entries) {
        sorted=sort_list(&head,name,num_entries,cap_file);
        spew_and_free_array(unmunged_name,sorted,num_entries);
    }

    fprintf(fd,"</UL>%c",LINEFEED);
    closedir(d);
}

#endif
