/*
 * str.c: string utility things
 * 
 * 3/21/93 Rob McCool
 * 
 */


#include <string.h>
#include "httpd.h"
#include <pwd.h>
#include <grp.h>

void strcpy_nocrlf(char *dest, char *src)
{
    register char c = *src;

    while((c!='\0') && (c!=LINEFEED) && (c!=CRETURN))
        c=(*dest++ = *src++);        

    if(c!='\0') *(dest-1)='\0';
}

void strsubfirst(int start,char *dest, char *src)
{
    char tmp[MAX_STRING_LEN];

    strcpy(tmp,&dest[start]);
    strcpy(dest,src);
    strcpy(&dest[strlen(src)],tmp);
}

/*
 * Parse .. so we don't compromise security
 */
void getparents(char *name)
{
    int l=0,w=0;
    const char *lookfor="..";

    while(name[l]!='\0') {
        if(name[l]!=lookfor[w]) (w>0 ? (l-=(w-1),w=0) : l++);
        else {
            if(lookfor[++w]=='\0') {
                if((name[l+1]=='\0') || (name[l+1]=='/')) {
                    register int m=l+1,n;

                    l=l-3;
                    if(l>=0) {
                        while((l!=0) && (name[l]!='/')) --l;
                    }
                    else l=0;
                    n=l;
                    while(name[n]=name[m]) (++n,++m);
                    w=0;
                }
                else w=0;
            }
            else ++l;
        }
    }
}
    
char *strsub(char *src, char *oldstr, char *newstr) {
    register char *t=src,*o,*nsrc=src;
    int oldlen,newlen;
    char *tosender;

    oldlen=strlen(oldstr);
    newlen=strlen(newstr);

/* If the new string is longer, assume the worst: our new string has to 
 * replace every letter in the source with the new. If not, don't thrash
 * malloc */
    if(oldlen<newlen) 
        nsrc=(char *)malloc(strlen(src)+(newlen-oldlen)*(strlen(src)/oldlen));

    tosender=nsrc;
    o=oldstr;
    while(*t) {
        if(*t != *o) *nsrc++ = *t++;
        else {
            register char *tmp=++t;
            register int n=oldlen-1;

            ++o; /* advance o (we advanced t above) */
            while((*tmp++ == *o++) && n) {
                --n;
            }
            tmp=newstr;
            if(!n) {     /* we matched */
                while(*nsrc++ = *tmp++);
                t+=oldlen-1;nsrc--;
            }
            else *nsrc++ = *(t-1); /* we missed, so put the char into new str */
            o=oldstr; /* no matter what, we damaged o */
        }
    }
    *nsrc='\0';
    /* not calling free is very anti-social */
    if(oldlen<newlen) free(src);
    return tosender;
}

void make_full_path(char *src1,char *src2,char *dst) {
    while(*dst++ = *src1++);
    *(dst-1)='/';
    while(*dst++ = *src2++);
}

int is_directory(char *path) {
    struct stat finfo;

    if(stat(path,&finfo) == -1)
        return 0; /* in error condition, just return no */

    return(S_ISDIR(finfo.st_mode));
}

char *full_hostname (void)
{
#ifdef HARDCODED_HOSTNAME
  return HARDCODED_HOSTNAME;
#else
  char str[128];
  int len = 128;

  gethostname (str, len);
  return gethostbyname(str)->h_name;
#endif
}

#ifdef NEED_STRDUP
char *strdup (char *str)
{
  char *dup;

  dup = (char *)malloc (strlen (str) + 1);
  dup = strcpy (dup, str);

  return dup;
}
#endif

uid_t uname2id(char *name) {
    struct passwd *ent;

    if(name[0] == '#') 
        return(atoi(&name[1]));

    if(!(ent = getpwnam(name)))
        server_error(stdout,BAD_USERNAME);

    else return(ent->pw_uid);
}

gid_t gname2id(char *name) {
    struct group *ent;

    if(name[0] == '#') 
        return(atoi(&name[1]));

    if(!(ent = getgrnam(name))) 
        server_error(stdout,BAD_GROUPNAME);

    else return(ent->gr_gid);
}
