/*
 * http_alias.c: Stuff for dealing with directory aliases
 * 
 * Rob McCool
 * 
 */


#include "httpd.h"

typedef struct {
    char fake[MAX_STRING_LEN];
    char real[MAX_STRING_LEN];
    int script;
} alias;

static int num_a = 0;
static alias a[MAX_ALIASES];
static int num_v = 0;
static alias v[MAX_ALIASES];

/* To send stat() information to http_script.c */
int dirs_in_alias;

void reset_aliases() {
    num_a = 0;
    num_v = 0;
}

void add_alias(char *f, char *r, int is_script) {
    strcpy(a[num_a].fake,f);

    a[num_a].script = is_script;
    if(r[0] != '/') 
        make_full_path((is_script ? server_root : document_root),r,
                       a[num_a++].real);
    else
        strcpy(a[num_a++].real,r);
}

void add_redirect(char *f, char *url) {
    strcpy(v[num_v].fake,f);
    strcpy(v[num_v++].real,url);
}

int translate_name(char *name, FILE *fd) {
    register int x,l;
    char w[MAX_STRING_LEN];
    char t[MAX_STRING_LEN];
    struct passwd *pw;

    getparents(name);

    for(x=0;x<num_v;x++) {
        l=strlen(v[x].fake);
        if(!strncmp(name,v[x].fake,l)) {
            strsubfirst(l,name,v[x].real);
            return REDIRECT_URL;
        }
    }

    for(x=0; x < num_a; x++) {
        l=strlen(a[x].fake);
        if(!strncmp(name,a[x].fake,l)) {
            strsubfirst(l,name,a[x].real);
            dirs_in_alias = count_dirs(a[x].real);
            return(a[x].script);
        }
    }

    if((user_dir[0]) && (name[0] == '/') && (name[1] == '~')) {
        char fake[MAX_STRING_LEN],real[MAX_STRING_LEN],dname[MAX_STRING_LEN];

        strcpy(dname,&name[2]);
        getword(w,dname,'/');
        if(!(pw=getpwnam(w)))
            die(NOT_FOUND,name,fd);
        fake[0] = '/';
        fake[1] = '~';
        strcpy(&fake[2],w);
        make_full_path(pw->pw_dir,user_dir,real);
        add_alias(fake,real,STD_DOCUMENT);
        strsubfirst(strlen(w) + 2,name,real);
        return STD_DOCUMENT;
    }
    /* no alias, add document root */
    strsubfirst(0,name,document_root);
    return STD_DOCUMENT;
}

void unmunge_name(char *name) {
    register int x,l;

    l=strlen(document_root);
    if(!strncmp(name,document_root,l)) {
        strsubfirst(l,name,"");
        if(!name[0]) {
            name[0] = '/';
            name[1] = '\0';
        }
        return;
    }
    for(x=0;x < num_a; x++) {
        l=strlen(a[x].real);
        if(!strncmp(name,a[x].real,l)) {
            strsubfirst(l,name,a[x].fake);
            if(!name[0]) {
                name[0] = '/';
                name[1] = '\0';
            }
            return;
        }
    }
}
