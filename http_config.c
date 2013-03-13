/*
 * http_config.c: auxillary functions for reading httpd's config file
 * and converting filenames into a namespace
 *
 * Rob McCool 3/21/93
 * 
 */

#include "httpd.h"

typedef struct {
    char name[MAX_STRING_LEN];
    char real[MAX_STRING_LEN];
} alias;

static int num_aliases;
alias aliases[MAX_ALIASES];


char original_name[MAX_STRING_LEN]; /* !!! FIX THIS !!! */

void unmunge_name(char *name)
{
    strcpy(name,original_name);
}

void translate_name(char *name)
{
    register int x,l;

    strcpy(original_name,name);

    for(x=0;x<num_aliases;x++) {
        l=strlen(aliases[x].name);
        if(!(strncmp(name,aliases[x].name,l)))
            strsubfirst(l,name,aliases[x].real);
    }
}

/* debugging */
void dump_aliases() 
{
    register int x;
    char blorf[MAX_STRING_LEN];

    for(x=0;x<num_aliases;x++)
        printf("%s:%s\n",aliases[x].name,aliases[x].real);
    strcpy(blorf,"./../.././.././.././../etc/passwd");
    translate_name(blorf);
    printf("translated %s to %s\n","./../.././.././.././../etc/passwd",blorf);
    strcpy(blorf,"../.././../../../../../././../././etc/passwd");
    translate_name(blorf);
}

int read_config(char *name, FILE *errors)
{
    FILE *cfg;
    char config_line[MAX_STRING_LEN];

    if(!(cfg=fopen(name,"r")))
        server_error(errors,CONFIG_FILENAME);

    num_aliases=0;
    while(fgets(config_line,MAX_STRING_LEN,cfg))
        if((config_line[0] != '\0') && (config_line[0] != '#') && 
           (config_line[0] != '\n')) {
            register int x=0,y,z;

            x=0;
            while((config_line[x] != '\0') && (config_line[x] != ':'))
                x++;
            if(config_line[x] == '\0')
                server_error(errors,CONFIG_SYNTAX);

            config_line[x]='\0';
            strcpy(aliases[num_aliases].name,config_line);

            y=x+1;
            if(config_line[y]=='\0') server_error(errors,CONFIG_SYNTAX);

            while(config_line[y] != '\0') y++;
            if(config_line[y-1]=='\n')
                config_line[--y]='\0';
            strcpy_nocrlf(aliases[num_aliases].real,&config_line[x+1]);

            if((x==1) && (config_line[y-1] != '/')) {
                aliases[num_aliases].real[y-2]='/'; /* y-x-1 */
                aliases[num_aliases].real[y]='\0'; /* y-x-1 +1 */
            }
            translate_name(aliases[num_aliases].name);
            num_aliases++;
        }
}

