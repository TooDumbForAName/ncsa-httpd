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
    struct tm *t;
    static char ts[MAX_STRING_LEN];

    t = gmtime(&sec);
    /* check return code? */
    strftime(ts,MAX_STRING_LEN,"%A, %d-%h-%y %T GMT",t);
    return ts;
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


void getline_timed_out() {
    char errstr[MAX_STRING_LEN];

    sprintf(errstr,"timed out waiting for %s",remote_name);
    log_error(errstr);
    fclose(stdin);
    fclose(stdout);
    exit(0);
}

int getline(char *s, int n, int f, unsigned int timeout) {
    register int i=0;

    signal(SIGALRM,getline_timed_out);
    alarm(timeout);
    while(1) {
        if(read(f,&s[i],1) <= 0) {
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

    return(((struct sockaddr_in *)&addr)->sin_port);
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
    
    remote_ip = inet_ntoa(*iaddr);
    if(!remote_host)
        remote_name = remote_ip;
}

char *get_remote_logname(FILE *fd) {
    int len;
    char *result;
#ifdef NEXT
    struct sockaddr_in sa_server;
    struct sockaddr sa_client;
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
        if(p=gethostbyname(str))
            server_hostname = strdup(p->h_name);
        else {
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
