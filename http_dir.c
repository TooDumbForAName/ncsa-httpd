/*
 * http_dir.c: Handles the on-the-fly html index generation
 * 
 * Rob McCool
 * 3/23/93
 * 
 */


/* httpd.h includes proper directory file */
#include "httpd.h"

void output_directories(char **ar,int n,char *name,FILE *fd)
{
    int x,lnum=0;

    if(!strcmp(name,"/")) name[0]='\0';
    for(x=0;x<n;x++) {
        if(strcmp(ar[x],".")) {
            fprintf(fd,"<LI> <A NAME=%d HREF=\"",++lnum);
            if(strcmp(ar[x],".."))
              {
                if (name[strlen(name)-1] == '/')
                  fprintf(fd,"%s%s\">%s</A>",name,ar[x],ar[x]);
                else
                  fprintf(fd,"%s/%s\">%s</A>",name,ar[x],ar[x]);
              }
            else {
                char blorf[MAX_STRING_LEN];
                strcpy(blorf,name);
                if (name[strlen(name)-1] == '/')
                  strcat(blorf,"..");
                else
                  strcat(blorf,"/..");
                getparents(blorf);
                if(blorf[0]=='\0') {
                    blorf[0]='/';
                    blorf[1]='\0';
                }
                fprintf(fd,"%s\">%s</A>",blorf,"Parent Directory");
            }
            fputc(LINEFEED,fd);
        }
    }
}    

int dsortf(char **s1,char **s2)
{
    return(strcmp(*s1,*s2));
}

    
void index_directory(char *name, FILE *fd)
{
    DIR *d;
    struct DIR_TYPE *dstruct;
    int num_ent=0,x;
    struct ent *head,*p,*q;
    char **ar;
    char unmunged_name[MAX_STRING_LEN];

    strcpy(unmunged_name,name);
    unmunge_name(unmunged_name);

#ifdef GROK_GOPHER
    if(!strncmp(unmunged_name,GOPHER_LOCATION,GOPHER_LOC_LEN)) {
        gopher_index(name,fd);
        return;
    }
#endif

    if(!(d=opendir(name)))
        client_error(fd,BAD_FILE);

/* MUST RE-TRANSLATE DIRECTORY NAME SO USER DOES NOT SEE TRUE NAMES */
    strcpy(name,unmunged_name);

/* Spew HTML preamble */
    fprintf(fd,"<TITLE>Index of %s</TITLE>",name);
    fputc(LINEFEED,fd);
    fprintf(fd,"<H1>Index of %s</H1>",name);
    fputc(LINEFEED,fd);

    fprintf(fd,"<UL>");
    fputc(LINEFEED,fd);

/* 
 * Since we don't know how many dir. entries there are, put them into a 
 * linked list and then arrayificate them so qsort can use them. 
 */
    head=(struct ent *)malloc(sizeof(struct ent));
    q=head;
    while(dstruct=readdir(d)) {
        p=(struct ent *)malloc(sizeof(struct ent));
        p->name=strdup(dstruct->d_name);
        p->next=NULL;
        q->next=p;
        q=p;
        num_ent++;
    }
    ar=(char **) malloc(num_ent*sizeof(char *));
    p=head;
    x=0;
    while(p=p->next)
        ar[x++]=p->name;
    
    qsort((void *)ar,num_ent,sizeof(char *),
#ifdef ULTRIX_BRAIN_DEATH
          (int (*))dsortf);
#else
          (int (*)(const void *,const void *))dsortf);
#endif

    output_directories(ar,num_ent,name,fd);
    free(ar);
    p=head->next;
    q=head;
    while(p=p->next) {
        free(q->name);
        free(q);
        q=p;
    }
    closedir(d);
    fprintf(fd,"</UL>");
}
