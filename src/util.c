/*
 * util.c: string utility things, and other utilities
 * 
 * All code contained herein is covered by the Copyright as distributed
 * in the README file in the main directory of the distribution of 
 * NCSA HTTPD.
 *
 * 03-23-93  Rob McCool
 * 	Original code up to version 1.3 from Rob McCool
 *
 * 02-16-95  cvarela
 *	Fixed stack hole in strsubfirst
 *
 * 03-06-95  blong
 *	Added inststr from bdflush-1.5 for Linux to set the name of
 *	the running processes
 *
 * 03-10-95  blong
 *	Added buffered getline for all but POST requests as suggested by
 *	Robert S. Thau (rst@ai.mit.edu)
 *
 * 03-20-95  blong & cvarela
 *	Fixed make_env_str so that it doesn't modify the pointers
 *
 * 04-03-95  blong
 *	May have fixed problems (esp. under Solaris 2.3) with playing
 *	with library memory in get_remote_name
 *
 * 04-13-95  guillory
 *	added strncpy_dir that limits the length of a directory copy
 *	also added strncpy for same reason
 *
 * 04-29-95  blong
 *	added patch by Kevin Steves (stevesk@mayfield.hp.com) for inststr
 *	under HPUX which uses the pstat command
 * 
 * 06-01-95  blong
 *	added patch by Vince Skahan (vds7789@aw101.iasl.ca.boeing.com)
 *	to fix Apollo DomainOS timezone handling
 */


#include "httpd.h"
#ifdef APOLLO
# include <sys/time.h>
#endif
#include <setjmp.h>
#ifdef HPUX
# include <sys/pstat.h>
#endif

extern JMP_BUF jmpbuffer;
extern char** environ;

/* modified from bdflush-1.5 for Linux source code 
   This is used to set the name of the running process */
void inststr(char *dst[], int argc, char *src)
{

#ifdef HPUX
 /*
  * 4/29/95 Kevin Steves <stevesk@mayfield.hp.com>
  * Use pstat(PSTAT_SETCMD) on HP-UX.
  */
     union pstun pst;
 
     pst.pst_command = src;
     pstat(PSTAT_SETCMD, pst, 0, 0, 0);
#else

    if (strlen(src) <= strlen(dst[0]))
    {
        char *ptr;

        for (ptr = dst[0]; *ptr; *(ptr++) = '\0');

        strcpy(dst[0], src);
    } else
    {
        /* stolen from the source to perl 4.036 (assigning to $0) */
        char *ptr, *ptr2;
        int count;
        ptr = dst[0] + strlen(dst[0]);
        for (count = 1; count < argc; count++) {
            if (dst[count] == ptr + 1)
                ptr += strlen(++ptr);
        }
        if (environ[0] == ptr + 1) {
            for (count = 0; environ[count]; count++)
                if (environ[count] == ptr + 1)
                    ptr += strlen(++ptr);
        }
        count = 0;
        for (ptr2 = dst[0]; ptr2 <= ptr; ptr2++) {
            *ptr2 = '\0';
            count++;
        }
        strncpy(dst[0], src, count);
    }
#endif
}

char *get_time() {
    time_t t;
    char *time_string;

    t=time(NULL);
    time_string = ctime(&t);
    time_string[strlen(time_string) - 1] = '\0';
    return (time_string);
}

char *gm_timestr_822(time_t sec) {
    return ht_time(sec,HTTP_TIME_FORMAT, 1);
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
#ifdef APOLLO
    struct timeval tp;	/* see gettimeofday(2) */
    struct timezone tzp;
#endif

    tt = time(NULL);
    t = localtime(&tt);
#if defined(BSD) && !defined(AUX) && !defined(APOLLO) && !defined(__QNX__)
    *tz = t->tm_gmtoff;
#else
  #ifdef APOLLO
    gettimeofday(&tp,&tzp);
    *tz = (60 * tzp.tz_minuteswest);
  #else
    *tz = - timezone;
  #endif
    if(t->tm_isdst)
        *tz += 3600;
#endif
    return t;
}


static char *months[] = {
    "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"
};


int find_month(char *mon) {
    register int x;

    for(x=0;x<12;x++)
        if(!strcmp(months[x],mon))
            return x;
    return -1;
}

/* Roy owes Rob beer. */
/* This would be considerably easier if strptime or timegm were portable */

int later_than(struct tm *lms, char *ims) {
    char *ip;
    char mname[MAX_STRING_LEN];
    int year = 0, month = 0, day = 0, hour = 0, min = 0, sec = 0, x;

    /* Whatever format we're looking at, it will start with weekday. */
    /* Skip to first space. */
    if(!(ip = strchr(ims,' ')))
        return 0;
    else
        while(isspace(*ip))
            ++ip;

    if(isalpha(*ip)) {
        /* ctime */
        sscanf(ip,"%s %d %d:%d:%d %*s %d",mname,&day,&hour,&min,&sec,&year);
    }
    else if(ip[2] == '-') {
        /* RFC 850 (normal HTTP) */
        char t[MAX_STRING_LEN];
        sscanf(ip,"%s %d:%d:%d",t,&hour,&min,&sec);
        t[2] = '\0';
        day = atoi(t);
        t[6] = '\0';
        strcpy(mname,&t[3]);
        x = atoi(&t[7]);
        /* Prevent wraparound from ambiguity */
        if(x < 70)
            x += 100;
        year = 1900 + x;
    }
    else {
        /* RFC 822 */
        sscanf(ip,"%d %s %d %d:%d:%d",&day,mname,&year,&hour,&min,&sec);
    }
    month = find_month(mname);

    if((x = (1900+lms->tm_year) - year))
        return x < 0;
    if((x = lms->tm_mon - month))
        return x < 0;
    if((x = lms->tm_mday - day))
        return x < 0;
    if((x = lms->tm_hour - hour))
        return x < 0;
    if((x = lms->tm_min - min))
        return x < 0;
    if((x = lms->tm_sec - sec))
        return x < 0;

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
  int src_len, dest_len, i;

  if ((src_len=strlen(src))<start){  /** src "fits" in dest **/
    for (i=0;dest[i]=src[i];i++);
    for (i=src_len;dest[i]=dest[i-src_len+start];i++);
  }
  else {                             /** src doesn't fit in dest **/
    for (dest_len=strlen(dest),i=dest_len+src_len-start;i>=src_len;i--)
      dest[i] = dest[i-src_len+start];
    for (i=0;i<src_len;i++) dest[i]=src[i];
  }
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

/*
 * A version of strcpy_dir that limits the number of characters
 * that will be copied to n - 1. If s is of length n or greater,
 * that portion is not copied and d is null-terminated at position
 * n. This fixes potential security hole in evaluate_acess.
 * SSG 4/15/95
 */
void strncpy_dir(char *d, char *s, int n) {
    register int x;
    
    for(x=0;s[x] && x < (n - 1);x++)
        d[x] = s[x];

    if(s[x-1] != '/' && x < (n - 1)) d[x++] = '/';
    d[x] = '\0';
}

/*
 * My version of strncpy. This will null terminate d if
 * s is n characters or longer. It also will only copy
 * n - 1 characters to d. SSG 4/13/95
 */
void lim_strcpy(char *d, char *s, int n) 
{
    while (--n && (*d++ = *s++))
     ;

    if (!n) 
      *d = '\0';
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
    if (!standalone) {
	fclose(stdin);
	fclose(stdout);
	exit(0);
    } else {
        if (remote_name) {
	    free(remote_name);
	    remote_name = NULL;
        }
        if (remote_host) {
	    free(remote_host);
	    remote_host = NULL;
        }
/* Don't free it, its system memory */ 
/*        if (remote_ip) {
	    free(remote_ip);
	    remote_ip = NULL;
        } */
#if defined(NeXT) || defined(__mc68000__)
	longjmp(jmpbuffer,1);
#else
        siglongjmp(jmpbuffer,1);
#endif
    }
}

/* Original, mostly brain dead version

int getline(char *s, int n, int f, unsigned int timeout) {
    register int i=0, ret;

    signal(SIGALRM,getline_timed_out);
    alarm(timeout);
    while(1) { 
        if((ret = read(f,&s[i],1)) <= 0) {
            /* Mmmmm, Solaris.  */
/*            if((ret == -1) && (errno == EINTR))
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
} */

/* Fixed, from Robert Thau
/* An awkward attempt to fix the single-character-read braindamage
 * which Rob McCool has described as the worst implementation decision
 * of his entire life...
 *
 * Unfortunately, there is no easy fix for CGI POSTs unless we want to
 * break NPH scripts (which we might).  However, all current GET
 * transactions can be buffered without fear.  So, let's try that and
 * see what happens.
 *
 * If the first four characters read off the socket are "GET ", we do
 * buffering.  Otherwise, we keep the original brain-damaged behavior.
 *
 * rst, 11/4/94
 *
 * Further hacked to not set the timeout alarm unless we are actually
 * reading from the socket.  (All those alarm() calls do add up).
 *
 * rst, 1/23/95
 */

int getline_seen_cmd = 0;
char getline_buffer[HUGE_STRING_LEN];
int getline_buffered_fd = -1;

static int getline_buf_posn;
static int getline_buf_good;

int getline_read_buf(char *s, int n, unsigned int timeout)
{
  char *endp = s + n - 1;
  int have_alarmed = 0;
  int buf_posn = getline_buf_posn, buf_good = getline_buf_good;
  int f = getline_buffered_fd;
  int client_broke_off = 0;
  int c = 0;                    /* Anything but LF */
  
  do
  {
    if (buf_posn == buf_good)
    {
      int ret;

      have_alarmed = 1;
      signal(SIGALRM,getline_timed_out);
      alarm(timeout);
      
      if ((ret=read(f, getline_buffer, sizeof(getline_buffer))) <= 0)
      {
        if (ret == -1 && errno == EINTR) continue; /* Solaris... */
        else { client_broke_off = 1; break; }
      }

      buf_good = ret;
      buf_posn = 0;
    }
    
    c = getline_buffer[buf_posn++];

    if (c == LF) break;
    if (c != CR) *s++ = c;
  }
  while (s < endp);
  
  if (have_alarmed) { alarm(0); signal(SIGALRM,SIG_IGN); }
  
  *s = '\0';
  getline_buf_posn = buf_posn;
  getline_buf_good = buf_good;
  
  return client_broke_off;
}

int getline(char *s, int n, int f, unsigned int timeout) {
    register int i=0, ret;

    if (f == getline_buffered_fd)
      return getline_read_buf (s, n, timeout);
    
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
            continue;           /* Get another character... */

        if((s[i] == LF) || (i == (n-1))) {
            alarm(0);
            signal(SIGALRM,SIG_IGN);
            s[i] = '\0';
            return 0;
        }

        if (i == 3 && getline_seen_cmd == 0)
        {
          getline_seen_cmd = 1;
          
          if (!strncmp (s, "GET ", 4)) {
            getline_buffered_fd = f;
            getline_buf_posn = getline_buf_good = 0; /* Force read */
            ret = getline_read_buf (s + 4, n - 4, timeout);
            return ret;
          }
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

int eat_ws (FILE* fp)
{
    int ch;

    while ((ch = fgetc (fp)) != EOF) {
        if (ch != ' ' && ch != '\t')
            return ch;
    }
    return ch;
}
         
int cfg_getline (char* s, int n, FILE* fp)
{
    int   len = 0, ch;

    ch = eat_ws(fp);
    while (1) {
        if (ch == EOF || ch == '\n' || (len == n-1)) {
            if (len && s[len - 1] == ' ') s[len - 1] = '\0'; 
            else s[len] = '\0';
            return feof(fp) ? 1 : 0;
        }
        s[len++] = ch;
        ch = fgetc (fp);
        if (ch == '\t' || ch == ' ') {
            s[len++] = ch;
            ch = eat_ws (fp);
        }
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

void escape_uri(char *url) {
    register int x,y;
    char *copy;

    copy = strdup(url);
            
    for(x=0,y=0;copy[x];x++,y++) {
        if(ind(":% ?+&",url[y] = copy[x]) != -1) {
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
    char *value2;
    char *name2;
    
    value2 = value;
    name2 = name;

    if(!(t = (char *)malloc(strlen(name)+strlen(value2)+2)))
        die(NO_MEMORY,"make_env_str",out);

    for(tp=t;*tp = *name2;tp++,name2++);
    for(*tp++ = '=';*tp = *value2;tp++,value2++);
    return t;
}

char* replace_env_str(char **env, char *name, char *value, FILE *out)
{
    register int i, len;
 
    for (i = 0, len = strlen(name); env[i]; i++) {
	if (strncmp(env[i], name, len) == 0) {
	    free(env[i]);
	    env[i] = make_env_str(name, value, out);
	    return env[i];
	}
    }
}


char **new_env(char **env, int to_add, int *pos) {
    if(!env) {
        char **newenv;
        *pos = 0;
        newenv = (char **)malloc((to_add+1)*sizeof(char *));
	newenv[to_add] = NULL;
	return newenv;
    }
    else {
        int x;

        for(x=0;env[x];x++)
	    ;

        if(!(env = (char **)realloc(env, (to_add+x+1)*(sizeof(char *)))))
            return NULL;

	env[to_add + x] = NULL;
        *pos = x;
        return env;
    }
}

void free_env(char **env) {
    int x;

    for(x=0;env[x];x++)
        free(env[x]);
    free(env);
    env = NULL;
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

#ifdef __QNX__
int setgroups(int index, gid_t *groups) {
   index = index;
   groups = groups;
   return 0;
}
#endif

#ifdef NEED_WAITPID
/* From ikluft@amdahl.com */
/* this is not ideal but it works for SVR3 variants */
/* httpd does not use the options so this doesn't implement them */
int waitpid(pid_t pid, int *statusp, int options)
{
    int tmp_pid;
    if ( kill ( pid,0 ) == -1) {
        errno=ECHILD;
        return -1;
    }
    while ((( tmp_pid = wait(statusp)) != pid) && ( tmp_pid != -1 ));
    return tmp_pid;
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
#ifndef MINIMAL_DNS
    struct hostent *hptr;
#endif

    len = sizeof(struct sockaddr);
    if (remote_name) {
	free(remote_name);
	remote_name = NULL;
    }
    if (remote_host) {
	free(remote_host);
	remote_host = NULL;
    }
/* Don't free it, its system memory */
/*    if (remote_ip) {
	free(remote_ip);
	remote_ip = NULL;
    } */

    if ((getpeername(fd, &addr, &len)) < 0) {
	remote_name = (char *) malloc (sizeof(char)*13);
        strcpy(remote_name,"UNKNOWN_HOST");
        return;
    }

    iaddr = &(((struct sockaddr_in *)&addr)->sin_addr);
#ifndef MINIMAL_DNS
    hptr = gethostbyaddr((char *)iaddr, sizeof(struct in_addr), AF_INET);
    if(hptr) {
        remote_host = strdup(hptr->h_name);
        str_tolower(remote_host);
	if (remote_name) free(remote_name);
        remote_name = strdup(remote_host);
    }
    else 
#endif
        remote_host = NULL;

#ifdef MAXIMUM_DNS
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
	    if (remote_host) {
		free(remote_host);
            	remote_host = NULL;
	    }
    }
#endif
    remote_ip = inet_ntoa(*iaddr);
    if(!remote_host){
	if (remote_name) free(remote_name);
        remote_name = strdup(remote_ip);
    }
    if (!remote_name){
	remote_name = (char *) malloc (sizeof(char)*15);
        strcpy(remote_name,"UNKNOWN_HOST");
    }
}

char *get_remote_logname(FILE *fd) {
    int len;
    char *result;
#if defined(NeXT) || defined(LINUX) || defined(SOLARIS2) || defined(__bsdi__) || defined(AIX4)
    struct sockaddr sa_server, sa_client;
#else
    struct sockaddr_in sa_server,sa_client;
#endif

    len = sizeof(sa_client);
    if(getpeername(fileno(stdout),&sa_client,&len) != -1) {
        len = sizeof(sa_server);
        if(getsockname(fileno(stdout),&sa_server,&len) == -1){
	    result = (char *) malloc(sizeof(char)*8);
            strcpy(result, "unknown");
	}
        else
            result = rfc931((struct sockaddr_in *) & sa_client,
                                    (struct sockaddr_in *) & sa_server);
    }
    else {
	result = (char *) malloc(sizeof(char)*8);
        strcpy(result, "unknown");
    }

    return result; /* robm=pinhead */
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
/* Since 80 is default port */
  if (port == 80) {
    sprintf(d,"http://%s%s",server_hostname,s);
  } else {
    sprintf(d,"http://%s:%d%s",server_hostname,port,s);
  }
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
