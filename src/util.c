/*
 * str.c: string utility things
 * 
 * 3/21/93 Rob McCool
 * 
 */


#include "httpd.h"

char *get_time() {
    time_t t;
    char *time_string;

    t=time(NULL);
    time_string = ctime(&t);
    time_string[strlen(time_string) - 1] = '\0';
    return (time_string);
}

char *gm_timestr_822(time_t sec) {
    /* HUH??? Why is the GMT hardcode necessary? */
    return ht_time(sec,"%A, %d-%h-%y %T GMT", 1);
}

char *ht_time(time_t t, char *fmt, int gmt) {
    static char ts[MAX_STRING_LEN];
    struct tm *tms;

    tms = (gmt ? gmtime(&t) : localtime(&t));

    /* check return code? */
    strftime(ts,MAX_STRING_LEN,fmt,tms);
    return ts;
}

/* What a pain in the ass. */
struct tm *get_gmtoff(long *tz) {
    time_t tt;
    struct tm *t;

    tt = time(NULL);
    t = localtime(&tt);
#ifdef BSD
    *tz = t->tm_gmtoff;
#else
    *tz = - timezone;
    if(t->tm_isdst)
        *tz += 3600;
#endif
    return t;
}

/* What another pain in the ass. */

static char *months[] = {
    "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"
};

static int ydays[] = {
    0,31,59,90,120,151,181,212,243,273,304,334
};

int find_month(char *mon) {
    register int x;

    for(x=0;x<12;x++)
        if(!strcmp(months[x],mon))
            return x;
    return -1;
}

#define find_yday(mon,day) (ydays[mon] + day)

int later_than(char *last_modified, char *ims) {
    char idate[MAX_STRING_LEN], ldate[MAX_STRING_LEN];
    char itime[MAX_STRING_LEN], ltime[MAX_STRING_LEN];
    char w[MAX_STRING_LEN];
    int lday,lmon,lyear, iday,imon,iyear, lhour,lmin, ihour,imin, x;
    long lsec, isec;

    sscanf(ims,"%*s %s %s",idate,itime);
    sscanf(last_modified,"%*s %s %s",ldate,ltime);

    getword(w,idate,'-');
    sscanf(w,"%d",&iday);
    getword(w,ldate,'-');
    sscanf(w,"%d",&lday);

    getword(w,idate,'-');
    imon = find_month(w);
    getword(w,ldate,'-');
    lmon = find_month(w);

    sscanf(idate,"%d",&iyear);
    sscanf(ldate,"%d",&lyear);

    x = lyear - iyear;
    if(x > 0) return 0;
    if(x < 0) return 1;

    x = find_yday(lmon, lday) - find_yday(imon,iday);
    if(x > 0) return 0;
    if(x < 0) return 1;

    sscanf(itime,"%d:%d:%ld",&ihour,&imin,&isec);
    sscanf(ltime,"%d:%d:%ld",&lhour,&lmin,&lsec);

    isec += (imin*60) + (ihour*3600);
    lsec += (lmin*60) + (lhour*3600);

    x = lsec - isec;
    if(x > 0) return 0;

    return 1;
}




/* Match = 0, NoMatch = 1, Abort = -1 */
/* Based loosely on sections of wildmat.c by Rich Salz */
int strcmp_match(char *str, char *exp) {
    int x,y;

    for(x=0,y=0;exp[y];++y,++x) {
        if((!str[x]) && (exp[y] != '*'))
            return -1;
        if(exp[y] == '*') {
            while(exp[++y] == '*');
            if(!exp[y])
                return 0;
            while(str[x]) {
                int ret;
                if((ret = strcmp_match(&str[x++],&exp[y])) != 1)
                    return ret;
            }
            return -1;
        } else 
            if((exp[y] != '?') && (str[x] != exp[y]))
                return 1;
    }
    return (str[x] != '\0');
}

int is_matchexp(char *str) {
    register int x;

    for(x=0;str[x];x++)
        if((str[x] == '*') || (str[x] == '?'))
            return 1;
    return 0;
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
                if((name[l+1]=='\0') || (name[l+1]=='/') &&
                   (((l > 3) && (name[l-2] == '/')) || (l<=3))) {
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

void no2slash(char *name) {
    register int x,y;

    for(x=0; name[x]; x++)
        if(x && (name[x-1] == '/') && (name[x] == '/'))
            for(y=x+1;name[y-1];y++)
                name[y-1] = name[y];
}

void make_dirstr(char *s, int n, char *d) {
    register int x,f;

    for(x=0,f=0;s[x];x++) {
        if((d[x] = s[x]) == '/')
            if((++f) == n) {
                d[x] = '\0';
                return;
            }
    }
    d[x] = '\0';
}

int count_dirs(char *path) {
    register int x,n;

    for(x=0,n=0;path[x];x++)
        if(path[x] == '/') n++;
    return n;
}


void strcpy_dir(char *d, char *s) {
    register int x;

    for(x=0;s[x];x++)
        d[x] = s[x];

    if(s[x-1] != '/') d[x++] = '/';
    d[x] = '\0';
}

void chdir_file(char *file) {
    int i;

    if((i = rind(file,'/')) == -1)
        return;
    file[i] = '\0';
    chdir(file);
    file[i] = '/';
}

void http2cgi(char *w) {
    register int x;

    for(x=strlen(w);x != -1; --x)
        w[x+5]= (w[x] == '-' ? '_' : toupper(w[x]));
    strncpy(w,"HTTP_",5);
}

void getline_timed_out() {
    char errstr[MAX_STRING_LEN];

    sprintf(errstr,"timed out waiting for %s",remote_name);
    log_error(errstr);
    fclose(stdin);
    fclose(stdout);
    exit(0);
}

int getline(char *s, int n, int f, unsigned int timeout) {
    register int i=0, ret;

    signal(SIGALRM,getline_timed_out);
    alarm(timeout);
    while(1) {
        if((ret = read(f,&s[i],1)) <= 0) {
            /* Mmmmm, Solaris.  */
            if((ret == -1) && (errno == EINTR))
                continue;
            s[i] = '\0';
            return 1;
        }

        if(s[i] == CR)
            read(f,&s[i],1);

        if((s[i] == LF) || (i == (n-1))) {
            alarm(0);
            signal(SIGALRM,SIG_IGN);
            s[i] = '\0';
            return 0;
        }
        ++i;
    }
}

void getword(char *word, char *line, char stop) {
    int x = 0,y;

    for(x=0;((line[x]) && (line[x] != stop));x++)
        word[x] = line[x];

    word[x] = '\0';
    if(line[x]) ++x;
    y=0;

    while(line[y++] = line[x++]);
}

void cfg_getword(char *word, char *line) {
    int x=0,y;
    
    for(x=0;line[x] && isspace(line[x]);x++);
    y=0;
    while(1) {
        if(!(word[y] = line[x]))
            break;
        if(isspace(line[x]))
            if((!x) || (line[x-1] != '\\'))
                break;
        if(line[x] != '\\') ++y;
        ++x;
    }
    word[y] = '\0';
    while(line[x] && isspace(line[x])) ++x;
    for(y=0;line[y] = line[x];++x,++y);
}

int cfg_getline(char *s, int n, FILE *f) {
    register int i=0;
    register char c;

    s[0] = '\0';
    /* skip leading whitespace */
    while(1) {
        c=(char)fgetc(f);
        if((c != '\t') && (c != ' '))
            break;
    }
    while(1) {
        if((c == '\t') || (c == ' ')) {
            s[i++] = ' ';
            while((c == '\t') || (c == ' ')) 
                c=(char)fgetc(f);
        }
        if(c == CR) {
            c = fgetc(f);
        }
        if((c == 0x4) || (c == LF) || (i == (n-1))) {
            /* blast trailing whitespace */
            while(i && (s[i-1] == ' ')) --i;
            s[i] = '\0';
            return (feof(f) ? 1 : 0);
        }
        s[i] = c;
        ++i;
        c = (char)fgetc(f);
    }
}

void escape_shell_cmd(char *cmd) {
    register int x,y,l;

    l=strlen(cmd);
    for(x=0;cmd[x];x++) {
        if(ind("&;`'\"|*?~<>^()[]{}$\\",cmd[x]) != -1){
            for(y=l+1;y>x;y--)
                cmd[y] = cmd[y-1];
            l++; /* length has been increased */
            cmd[x] = '\\';
            x++; /* skip the character */
        }
    }
}

void plustospace(char *str) {
    register int x;

    for(x=0;str[x];x++) if(str[x] == '+') str[x] = ' ';
}

void spacetoplus(char *str) {
    register int x;

    for(x=0;str[x];x++) if(str[x] == ' ') str[x] = '+';
}

char x2c(char *what) {
    register char digit;

    digit = ((what[0] >= 'A') ? ((what[0] & 0xdf) - 'A')+10 : (what[0] - '0'));
    digit *= 16;
    digit += (what[1] >= 'A' ? ((what[1] & 0xdf) - 'A')+10 : (what[1] - '0'));
    return(digit);
}

void unescape_url(char *url) {
    register int x,y;

    for(x=0,y=0;url[y];++x,++y) {
        if((url[x] = url[y]) == '%') {
            url[x] = x2c(&url[y+1]);
            y+=2;
        }
    }
    url[x] = '\0';
}

#define c2x(what,where) sprintf(where,"%%%2x",what)

void escape_url(char *url) {
    register int x,y;
    register char digit;
    char *copy;

    copy = strdup(url);
            
    for(x=0,y=0;copy[x];x++,y++) {
        if(ind("% ?+&",url[y] = copy[x]) != -1) {
            c2x(copy[x],&url[y]);
            y+=2;
        }
    }
    url[y] = '\0';
    free(copy);
}

void make_full_path(char *src1,char *src2,char *dst) {
    register int x,y;

    for(x=0;dst[x] = src1[x];x++);

    if(!x) dst[x++] = '/';
    else if((dst[x-1] != '/'))
        dst[x++] = '/';

    for(y=0;dst[x] = src2[y];x++,y++);
}

int is_directory(char *path) {
    struct stat finfo;

    if(stat(path,&finfo) == -1)
        return 0; /* in error condition, just return no */

    return(S_ISDIR(finfo.st_mode));
}

int is_url(char *u) {
    register int x;

    for(x=0;u[x] != ':';x++)
        if((!u[x]) || (!isalpha(u[x])))
            return 0;

    if((u[x+1] == '/') && (u[x+2] == '/'))
        return 1;
    else return 0;
}

char *make_env_str(char *name, char *value, FILE *out) {
    char *t,*tp;

    if(!(t = (char *)malloc(strlen(name)+strlen(value)+2)))
        die(NO_MEMORY,"make_env_str",out);

    for(tp=t;*tp = *name;tp++,name++);
    for(*tp++ = '=';*tp = *value;tp++,value++);
    return t;
}

char **new_env(char **env, int to_add, int *pos) {
    if(!env) {
        *pos = 0;
        return (char **)malloc((to_add+1)*sizeof(char *));
    }
    else {
        int x;
        char **newenv;

        for(x=0;env[x];x++);
        if(!(newenv = (char **)malloc((to_add+x+1)*(sizeof(char *)))))
            return NULL;
        for(x=0;env[x];x++)
            newenv[x] = env[x];
        *pos = x;
        free(env);
        return newenv;
    }
}

void free_env(char **env) {
    int x;

    for(x=0;env[x];x++)
        free(env[x]);
    free(env);
}

int can_exec(struct stat *finfo) {
    if(user_id == finfo->st_uid)
        if(finfo->st_mode & S_IXUSR)
            return 1;
    if(group_id == finfo->st_gid)
        if(finfo->st_mode & S_IXGRP)
            return 1;
    return (finfo->st_mode & S_IXOTH);
}

#ifdef NEED_STRDUP
char *strdup (char *str)
{
  char *dup;

  if(!(dup = (char *)malloc (strlen (str) + 1)))
      return NULL;
  dup = strcpy (dup, str);

  return dup;
}
#endif

/* The following two routines were donated for SVR4 by Andreas Vogel */
#ifdef NEED_STRCASECMP
int strcasecmp (const char *a, const char *b)
{
    const char *p = a;
    const char *q = b;
    for (p = a, q = b; *p && *q; p++, q++)
    {
      int diff = tolower(*p) - tolower(*q);
      if (diff) return diff;
    }
    if (*p) return 1;       /* p was longer than q */
    if (*q) return -1;      /* p was shorter than q */
    return 0;               /* Exact match */
}

#endif

#ifdef NEED_STRNCASECMP
int strncasecmp (const char *a, const char *b, int n)
{
    const char *p = a;
    const char *q = b;

    for (p = a, q = b; /*NOTHING*/; p++, q++)
    {
      int diff;
      if (p == a + n) return 0;     /*   Match up to n characters */
      if (!(*p && *q)) return *p - *q;
      diff = tolower(*p) - tolower(*q);
      if (diff) return diff;
    }
    /*NOTREACHED*/
}
#endif



#ifdef NEED_INITGROUPS
int initgroups(const char *name, gid_t basegid)
{
  gid_t groups[NGROUPS_MAX];
  struct group *g;
  int index = 0;

  groups[index++] = basegid;

  while (index < NGROUPS_MAX && ((g = getgrent()) != NULL))
    if (g->gr_gid != basegid)
    {
      char **names;

      for (names = g->gr_mem; *names != NULL; ++names)
        if (!strcmp(*names, name))
          groups[index++] = g->gr_gid;
    }

  return setgroups(index, groups);
}
#endif

int ind(char *s, char c) {
    register int x;

    for(x=0;s[x];x++)
        if(s[x] == c) return x;

    return -1;
}

int rind(char *s, char c) {
    register int x;

    for(x=strlen(s)-1;x != -1;x--)
        if(s[x] == c) return x;

    return -1;
}

void str_tolower(char *str) {
    while(*str) {
        *str = tolower(*str);
        ++str;
    }
}
        
uid_t uname2id(char *name) {
    struct passwd *ent;

    if(name[0] == '#') 
        return(atoi(&name[1]));

    if(!(ent = getpwnam(name))) {
        fprintf(stderr,"httpd: bad user name %s\n",name);
        exit(1);
    }
    else return(ent->pw_uid);
}

gid_t gname2id(char *name) {
    struct group *ent;

    if(name[0] == '#') 
        return(atoi(&name[1]));

    if(!(ent = getgrnam(name))) {
        fprintf(stderr,"httpd: bad group name %s\n",name);
        exit(1);
    }
    else return(ent->gr_gid);
}

int get_portnum(int sd,FILE *out) {
    struct sockaddr addr;
    int len;

    len = sizeof(struct sockaddr);
    if(getsockname(sd,&addr,&len) < 0)
        die(SERVER_ERROR,"could not get port number",out);
    return ntohs(((struct sockaddr_in *)&addr)->sin_port);
}

char *find_fqdn(struct hostent *p) {
    int x;

    if(ind(p->h_name,'.') == -1) {
        for(x=0;p->h_aliases[x];++x) {
            if((ind(p->h_aliases[x],'.') != -1) && 
               (!strncmp(p->h_aliases[x],p->h_name,strlen(p->h_name))))
                return strdup(p->h_aliases[x]);
        }
        return NULL;
    } else return strdup(p->h_name);
}

void get_remote_host(int fd) {
    struct sockaddr addr;
    int len;
    struct in_addr *iaddr;
    struct hostent *hptr;

    len = sizeof(struct sockaddr);

    if ((getpeername(fd, &addr, &len)) < 0) {
        remote_host=NULL;
        remote_ip=NULL;
        remote_name="UNKNOWN_HOST";
        return;
    }
    iaddr = &(((struct sockaddr_in *)&addr)->sin_addr);
    hptr = gethostbyaddr((char *)iaddr, sizeof(struct in_addr), AF_INET);
    if(hptr) {
        remote_host = strdup(hptr->h_name);
        str_tolower(remote_host);
        remote_name = remote_host;
    }
    else remote_host = NULL;

    /* Grrr. Check THAT name to make sure it's really the name of the addr. */
    /* Code from Harald Hanche-Olsen <hanche@imf.unit.no> */
    if(remote_host) {
        char **haddr;
        hptr = gethostbyname(remote_host);
        if (hptr) {
            for(haddr=hptr->h_addr_list;*haddr;haddr++) {
                if(((struct in_addr *)(*haddr))->s_addr == iaddr->s_addr)
                    break;
            }
        }
        if((!hptr) || (!(*haddr)))
            remote_host = NULL;
    }
    remote_ip = inet_ntoa(*iaddr);
    if(!remote_host)
        remote_name = remote_ip;
}

char *get_remote_logname(FILE *fd) {
    int len;
    char *result;
#ifdef NEXT
    struct sockaddr sa_server, sa_client;
#else
    struct sockaddr_in sa_server,sa_client;
#endif

    len = sizeof(sa_client);
    if(getpeername(fileno(stdout),&sa_client,&len) != -1) {
        len = sizeof(sa_server);
        if(getsockname(fileno(stdout),&sa_server,&len) == -1)
            result = "unknown";
        else
            result = rfc931((struct sockaddr_in *) & sa_client,
                                    (struct sockaddr_in *) & sa_server);
    }
    else result = "unknown";
}

void get_local_host()
{
    char str[128];
    int len = 128;

    if(!server_hostname) {
        struct hostent *p;
        gethostname(str, len);
        if((!(p=gethostbyname(str))) || (!(server_hostname = find_fqdn(p)))) {
            fprintf(stderr,"httpd: cannot determine local host name.\n");
            fprintf(stderr,"Use ServerName to set it manually.\n");
            exit(1);
        }
    }
}

void construct_url(char *d, char *s) {
    sprintf(d,"http://%s:%d%s",server_hostname,port,s);
/*    escape_url(d); */
}

/* aaaack but it's fast and const should make it shared text page. */
const int pr2six[256]={
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,62,64,64,64,63,
    52,53,54,55,56,57,58,59,60,61,64,64,64,64,64,64,64,0,1,2,3,4,5,6,7,8,9,
    10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,64,64,64,64,64,64,26,27,
    28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64
};

void uudecode(char *bufcoded, unsigned char *bufplain, int outbufsize) {
    int nbytesdecoded, j;
    register char *bufin = bufcoded;
    register unsigned char *bufout = bufplain;
    register int nprbytes;
    
    /* Strip leading whitespace. */
    
    while(*bufcoded==' ' || *bufcoded == '\t') bufcoded++;
    
    /* Figure out how many characters are in the input buffer.
     * If this would decode into more bytes than would fit into
     * the output buffer, adjust the number of input bytes downwards.
     */
    bufin = bufcoded;
    while(pr2six[*(bufin++)] <= 63);
    nprbytes = bufin - bufcoded - 1;
    nbytesdecoded = ((nprbytes+3)/4) * 3;
    if(nbytesdecoded > outbufsize) {
        nprbytes = (outbufsize*4)/3;
    }
    
    bufin = bufcoded;
    
    while (nprbytes > 0) {
        *(bufout++) = 
            (unsigned char) (pr2six[*bufin] << 2 | pr2six[bufin[1]] >> 4);
        *(bufout++) = 
            (unsigned char) (pr2six[bufin[1]] << 4 | pr2six[bufin[2]] >> 2);
        *(bufout++) = 
            (unsigned char) (pr2six[bufin[2]] << 6 | pr2six[bufin[3]]);
        bufin += 4;
        nprbytes -= 4;
    }
    
    if(nprbytes & 03) {
        if(pr2six[bufin[-2]] > 63)
            nbytesdecoded -= 2;
        else
            nbytesdecoded -= 1;
    }
    bufplain[nbytesdecoded] = '\0';
}
