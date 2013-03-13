/************************************************************************
 * NCSA HTTPd Server
 * Software Development Group
 * National Center for Supercomputing Applications
 * University of Illinois at Urbana-Champaign
 * 605 E. Springfield, Champaign, IL 61820
 * httpd@ncsa.uiuc.edu
 *
 * Copyright  (C)  1995, Board of Trustees of the University of Illinois
 *
 ************************************************************************
 *
 * util.c,v 1.115 1996/03/27 20:44:30 blong Exp
 *
 ************************************************************************
 *
 * util.c: string utility things, and other utilities
 * 
 *
 */


#include "config.h"
#include "portability.h"

#ifndef NO_STDLIB_H
# include <stdlib.h>
#endif /* NO_STDLIB_H */
#ifndef NO_MALLOC_H
# ifdef NEED_SYS_MALLOC_H
#  include <sys/malloc.h>
# else
#  include <malloc.h>
# endif /* NEED_SYS_MALLOC_H */
#endif /* NO_MALLOC_H */
#include <setjmp.h>
#include <signal.h>
#include <string.h>
#include <grp.h>
#include <pwd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>
#ifdef HPUX
# include <sys/pstat.h>
#endif
#ifdef APOLLO
# include <sys/time.h>
#endif
#include "constants.h"
#include "allocate.h"
#include "util.h"
#include "http_request.h"
#include "http_config.h"
#include "host_config.h"
#include "http_log.h"
#include "httpd.h"


#undef DONTCOMPILEIN
#ifdef DONTCOMPILEIN

/* Superseded by setproctitle (next function) */

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
    if (strlen(src) <= strlen(dst[0])) {
        char *ptr; /*= dst[0];*/

	for (ptr = dst[0]; *ptr; *(ptr++) = '\0');
	/*while (*ptr)
	    *ptr++ = '\0';*/

        strcpy(dst[0], src);
    } 
    else {
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

#endif /* 0 */

/*
 * initproctitle(),setproctitle() as given by 
 *    Kevin Ruddy (smiles@powerdog.com) 
 *
 * initproctitle() initializes variables and sets the prefix for the name
 * setproctitle() sets the title given by ps
 *
 * This is known to work under SunOS and AIX3.  There is a specific 
 * command under HPUX to make this work according to 
 * Kevin Steves <stevesk@mayfield.hp.com> which uses 
 * pstat(PSTAT_SETCMD) on HP-UX.  I think BSDI also has a setproctitle()
 * in its libc, but I don't know the interface to add it (from sendmail)
 *
 */

#ifdef SETPROCTITLE

static char     *procstart = NULL,
                *proctitle = NULL;
static int      procstartn = 0,
                proctitlen = 0;
 
void
initproctitle(char *start, int argc, char **argv, char **envp)
{
#ifndef HPUX
        char    *p;
        int     i;

        if (procstart != NULL)
                (void) free(procstart);
        procstart = (char *) malloc(strlen(start) + 3);
        (void) strcpy(procstart, start);
        (void) strcat(procstart, ": ");
        procstartn = strlen(procstart);

        if (argv != 0) {
                proctitle = argv[0];
                for (i = 0; envp[i] != 0; i++)
                        ;
                if (i > 0)
                        p = envp[i - 1] + strlen(envp[i - 1]);
                else
                        p = argv[argc - 1] + strlen(argv[argc - 1]);
                proctitlen = p - proctitle;
        }
#endif /* HPUX */
}

#ifndef HAVE_SETPROCTITLE
void setproctitle(char *title)
{
#ifdef HPUX
    /*
     * 4/29/95 Kevin Steves <stevesk@mayfield.hp.com>
     * Use pstat(PSTAT_SETCMD) on HP-UX.
     */
    union pstun pst;
    char tmp[HUGE_STRING_LEN];

    
    strcpy(tmp,process_name);
    strcat(tmp,": ");
    strncat(tmp,title,HUGE_STRING_LEN-strlen(tmp));
    pst.pst_command = tmp;
    pstat(PSTAT_SETCMD, pst, 0, 0, 0);
#else
        int     len;

        if (proctitle == 0 || proctitlen < procstartn)
                return;

        (void) memset(proctitle, ' ', proctitlen);
        (void) strncpy(proctitle, procstart, procstartn);
        len = strlen(title);
        if (len > proctitlen - procstartn)
                len = proctitlen - procstartn;
        (void) strncpy(proctitle + procstartn, title, len);
#endif /* HPUX */
}
#endif /* HAVE_SETPROCTITLE */

#endif /* SETPROCTITLE */

char *get_time(void) 
{
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
    struct timeval tp;        /* see gettimeofday(2) */
    struct timezone tzp;
#endif

    tt = time(NULL);
    t = localtime(&tt);
#if defined(BSD) && !defined(AUX) && !defined(APOLLO) && !defined(__QNX__) && !defined(CONVEXOS) && !defined(SCO) && !defined(SCO3)
    *tz = t->tm_gmtoff;
#elif defined(CONVEXOS) || defined(SCO3) || defined(SCO)
    {
        struct timeval tp;
        struct timezone tzp;
        gettimeofday(&tp, &tzp);
        *tz = tzp.tz_minuteswest * 60;
    }
#else
# ifdef APOLLO
    gettimeofday(&tp,&tzp);
    *tz = (60 * tzp.tz_minuteswest);
# else
      *tz = - timezone;
# endif
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
        sscanf(ip,"%s %d %d:%d:%d %d",mname,&day,&hour,&min,&sec,&year);
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
int strcmp_match(char *str, char *exp) 
{
    int x,y;

    for(x=0,y=0;exp[y];++y,++x) {
        if((!str[x]) && (exp[y] != '*'))
            return -1;
        if(exp[y] == '*') {
            while(exp[++y] == '*')
		;
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
    for (i=0;(dest[i]=src[i]);i++);
    for (i=src_len;(dest[i]=dest[i-src_len+start]);i++);
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

    no2slash(name);

    while(name[l]!='\0') {
        if(name[l]!=lookfor[w]) (w>0 ? (l-=(w-1),w=0) : l++);
        else {
            if(lookfor[++w]=='\0') {
                if(((name[l+1]=='\0') || (name[l+1]=='/')) &&
                   (((l > 3) && (name[l-2] == '/')) || (l<=3))) {
                    register int m=l+1,n;

                    l=l-3;
                    if(l>=0) {
                        while((l!=0) && (name[l]!='/')) --l;
                    }
                    else l=0;
                    n=l;
                    while((name[n]=name[m])) (++n,++m);
                    w=0;
                }
                else w=0;
            }
            else ++l;
        }
    }
}

void no2slash(char *name)
{
    register char *s;
    register char *p;

/* collapse //, ///, etc. into /
 */
    for (s = name; *s; s++)
	while (s[0] == '/' && s[1] == '/')
	{
	    p = s;
	    while ((p[0] = p[1]))
		p++;
	}

/* collapse /./ into /
 */
    for (s = name; *s; s++)
	while (s[0] =='/' && s[1] == '.' && s[2] == '/')
	{
	    p = s;
	    while ((p[0] = p[2]))
		p++;
	}

/* remove ./ at the beginning of the name */
    if (name[0] == '.' && name[1] == '/')
    {
	p = name;
	while ((p[0] = p[2]))
	    p++;
    }

/* change /. at the end of the name to /
 */
    if (name[0] && name[1])
	while (name[2])
	    name++;
    if (name[0] == '/' && name[1] == '.' && name[2] == '\0')
	name[1] = 0;
}

void make_dirstr(char *s, int n, char *d) {
    register int x,f;

    for(x=0,f=0;s[x];x++) {
        if((d[x] = s[x]) == '/')
            if((++f) == n) {
	      if(x == 0)
		x++;
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

    if(x && s[x-1] != '/' && x < (n - 1)) d[x++] = '/';
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

void http2cgi(char* h, char *w) {

    strncpy(h,"HTTP_",5);
    h += 5;
    while ((*h++ = (*w == '-') ? '_' : toupper(*w))) 
	w++;
}

void getline_timed_out(int sig) 
{
    char errstr[MAX_STRING_LEN];

    sprintf(errstr,"timed out waiting for %s", 
      gCurrentRequest->remote_name ? gCurrentRequest->remote_name : "-");
    log_error(errstr,gCurrentRequest->hostInfo->error_log);
    if (!standalone) {
	fclose(stdin);
	fclose(stdout);
	exit(0);
    } else {
#ifdef NO_SIGLONGJMP
	longjmp(jmpbuffer,1);
#else
        siglongjmp(jmpbuffer,1);
#endif
    }
}

sock_buf *new_sock_buf(per_request *reqInfo, int sd) 
{
   sock_buf *tmp;
   if (!(tmp = (sock_buf *)malloc(sizeof(sock_buf)))) {
     die(reqInfo,SC_NO_MEMORY,"new_sock_buf");
   }
   tmp->status = SB_NEW;
   tmp->sd = sd;

   return tmp;
}

/* Modified by Trung Dung (tdung@OpenMarket.com) to handle RFC822 
 * line wraps
 * This routine is currently not thread safe.
 * This routine may be thread safe. (blong 3/13/96)
 */
int getline(sock_buf *sb, char *s, int n, int options, unsigned int timeout)
{
    char *endp = s + n - 1;
    int have_alarmed = 0;
/*    static int buf_posn, buf_good; */
    int buf_start;
/*    static char buffer[HUGE_STRING_LEN]; */
    int c;
    int ret;
    int size;

    buf_start = sb->buf_posn;
    if ((options & G_RESET_BUF) || (sb->status == SB_NEW)) {
	buf_start = sb->buf_posn = sb->buf_good = 0;
	sb->buffer[0] = '\0';
    }
    else if (options & G_FLUSH) {
	while (sb->buf_posn < sb->buf_good) 
	    *s++ = sb->buffer[(sb->buf_posn)++];
	*s = '\0';
	sb->status = SB_FLUSHED;
	return sb->buf_posn - buf_start;
    }
    if (options & G_SINGLE_CHAR) {
      size = 1;
    } else {
      size = HUGE_STRING_LEN;
    }

    do {
	if (sb->buf_posn == sb->buf_good) {
	    have_alarmed = 1;
	    signal(SIGALRM,getline_timed_out);
	    alarm(timeout);

	    ret=read(sb->sd, sb->buffer, size);

	    if (ret <= 0) {
		if (ret == -1 && errno == EINTR) 
		    continue; /* Solaris... */
		else {
		    sb->status = SB_ERROR;
                    if (have_alarmed) { alarm(0); signal(SIGALRM,SIG_IGN); }
		    /* just always return -1, instead of 0 */
		    return -1;
		}
	    }
	    sb->status = SB_READ;

	    sb->buf_good = ret;
	    buf_start -= sb->buf_posn;
	    sb->buf_posn = 0;

	    		/* ADC hack below	ZZZ */
/*
	    if (ret >0) {
		for (c = 0; c < ret; c++)
		   fputc(sb->buffer[c],stderr);
	    	c = 0;
	    }
*/

	}
	
	c = sb->buffer[(sb->buf_posn)++];
        if ((c == '\r') && (sb->buf_posn + 1 < sb->buf_good) &&
	    (sb->buffer[sb->buf_posn] == '\n') && 
            ((sb->buffer[sb->buf_posn + 1] == ' ') || 
	     (sb->buffer[sb->buf_posn + 1] == '\t'))) 
        {
          *s++ = c;
          *s++ = '\n';
          *s++ = sb->buffer[sb->buf_posn + 1];
          sb->buf_posn += 2;
        }
        else if ((c == '\n') && (sb->buf_posn < sb->buf_good) &&
            ((sb->buffer[sb->buf_posn] == ' ') || 
	     (sb->buffer[sb->buf_posn] == '\t'))) 
	{
          *s++ = '\n';
          *s++ = sb->buffer[sb->buf_posn];
          sb->buf_posn += 1;
        }
        else if (c == LF) break;
        else if (c != CR) *s++ = c;
    } while (s < endp);
  
    if (have_alarmed) { alarm(0); signal(SIGALRM,SIG_IGN); }
  
    *s = '\0';

    return sb->buf_posn - buf_start;
}

void splitURL(char *line, char *url, char *args) {
  int x,y;
  int inURL = TRUE;

  for (x=0,y=0 ; ((line[x]) && (line[x] != ' ')); x++) {
    if (inURL) {
      if ((url[x] = line[x]) == '?') {
	url[x] = '\0';
	inURL = FALSE;
      }
    } else {
      args[y] = line[x];
      y++;
    }
  }
  args[y] = '\0';
}

void getword(char *word, char *line, char stop) {
    int x = 0,y;

    for(x=0;((line[x]) && (line[x] != stop));x++)
        word[x] = line[x];

    word[x] = '\0';
    if(line[x]) ++x;
    y=0;

    while((line[y++] = line[x++]));
}

void cfg_getword(char *word, char *line) 
{
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
    for(y=0;(line[y] = line[x]);++x,++y);
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
        if(ind("&;`'\"|*?~<>^()[]{}$\\\x0A",cmd[x]) != -1){
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
    char copy[HUGE_STRING_LEN];

    strncpy(copy,url,HUGE_STRING_LEN);
            
    for(x=0,y=0;copy[x];x++,y++) {
        if(ind("% ?+&",url[y] = copy[x]) != -1) {
            c2x(copy[x],&url[y]);
            y+=2;
        }
    }
    url[y] = '\0';
}

void escape_uri(char *url) {
    register int x,y;
    char copy[HUGE_STRING_LEN];

    strncpy(copy,url,HUGE_STRING_LEN);
            
    for(x=0,y=0;copy[x];x++,y++) {
        if(ind("#:% ?+&",url[y] = copy[x]) != -1) {
            c2x(copy[x],&url[y]);
            y+=2;
        }
    }
    url[y] = '\0';
}

void add_file_to_dir(char *dir,char *file) {

    if (dir[strlen(dir)-1] == '/') {
      if (file[0] == '/') strcat(dir,file+1);
	else strcat(dir,file);
    } else {
      if (file[0] == '/') strcat(dir,file);
	else sprintf(dir,"%s/%s",dir,file);
    }
}

void make_full_path(char *src1,char *src2,char *dst) {
    register int x = 0,y;

/* The following was suggested by djm@va.pubnix.com (David J. MacKenzie)
 * Not sure why yet, so I'll ask him
 */
/*  if (strcmp(src2,"/dev/null")) */
    for(x=0;(dst[x] = src1[x]);x++);

    if(!x) dst[x++] = '/';
    else if((dst[x-1] != '/'))
        dst[x++] = '/';

    for(y=0;(dst[x] = src2[y]);x++,y++);
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

/*
 * Return the position of character c in string s. Return -1 if c does
 * not occur in s.
 */

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

static
long
scan_long (char *s, long *n)
{
    char extra[2];

    *extra = 0;
    return (sscanf (s, "%ld%1s", n, extra) == 1 && *extra == 0);
}

uid_t uname2id(char *name) {
    struct passwd *ent;
    long id;

    if(name[0] == '#') 
    {
	if (!scan_long (&name[1], &id))
	{
	    fprintf(stderr,"HTTPd: bad user id %s\n",name);
	    exit(1);
	}
	return (uid_t) id ;
    }

    if(!(ent = getpwnam(name))) {
        fprintf(stderr,"HTTPd: bad user name %s\n",name);
        exit(1);
    }
    return(ent->pw_uid);
}

gid_t gname2id(char *name) {
    struct group *ent;
    long id;

    if(name[0] == '#') 
    {
	if (!scan_long (&name[1], &id))
	{
	    fprintf(stderr,"HTTPd: group id %s\n",name);
	    exit(1);
	}
	return (gid_t) id ;
    }

    if(!(ent = getgrnam(name))) {
        fprintf(stderr,"HTTPd: bad group name %s\n",name);
        exit(1);
    }
    return(ent->gr_gid);
}

int get_portnum(per_request *reqInfo, int sd) {
    struct sockaddr addr;
    int len;

    len = sizeof(struct sockaddr);
    if(getsockname(sd,&addr,&len) < 0)
        die(reqInfo, SC_SERVER_ERROR,"could not get port number");
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

int get_remote_host_min(per_request *reqInfo) {
    struct sockaddr addr;
    int len;
    struct in_addr *iaddr;
    struct hostent *hptr;

    len = sizeof(struct sockaddr);

    if ((getpeername(reqInfo->connection_socket, &addr, &len)) < 0) {
	return -1;
    }

    iaddr = &(((struct sockaddr_in *)&addr)->sin_addr);
    hptr = gethostbyaddr((char *)iaddr, sizeof(struct in_addr), AF_INET);
    if(hptr) {
      if (reqInfo->remote_host) {
	freeString(reqInfo->remote_host);
      }
      reqInfo->remote_host = dupStringP(hptr->h_name,STR_REQ);
      str_tolower(reqInfo->remote_host);
      if (reqInfo->remote_name) {
	freeString(reqInfo->remote_name);
      }
      reqInfo->remote_name = dupStringP(reqInfo->remote_host,STR_REQ);
    } else {
  	/* shouldn't be necessary, but just in case */
	if (reqInfo->remote_host) {
	  freeString(reqInfo->remote_host);
        }
	reqInfo->remote_host = NULL;
    }
    reqInfo->dns_host_lookup = TRUE;
    return 0;
}

void get_remote_host(per_request *reqInfo) 
{
    struct sockaddr addr;
    int len;
    struct in_addr *iaddr;
    struct hostent *hptr;

    len = sizeof(struct sockaddr);
    
    if ((getpeername(reqInfo->connection_socket, &addr, &len)) < 0) {
      reqInfo->remote_name = dupStringP("UNKNOWN_HOST",STR_REQ);
      reqInfo->remote_ip = dupStringP("UNKNOWN_IP",STR_REQ);
      reqInfo->remote_host = dupStringP("UNKNOWN_HOST",STR_REQ);
      return;
    }

    iaddr = &(((struct sockaddr_in *)&addr)->sin_addr);
    if ((reqInfo->hostInfo->dns_mode != DNS_MIN) &&
	(reqInfo->hostInfo->dns_mode != DNS_NONE))
    {
      hptr = gethostbyaddr((char *)iaddr, sizeof(struct in_addr), AF_INET);
      if(hptr) {
        reqInfo->remote_host = dupStringP(hptr->h_name,STR_REQ);
        str_tolower(reqInfo->remote_host);
	if (reqInfo->remote_name) {
	  freeString(reqInfo->remote_name);
        }
        reqInfo->remote_name = dupStringP(reqInfo->remote_host,STR_REQ);
      } else reqInfo->remote_host = NULL;
      reqInfo->dns_host_lookup = TRUE;
    } else {
	if (reqInfo->remote_host != NULL) freeString(reqInfo->remote_host);
        reqInfo->remote_host = NULL;
    }
    
    if (reqInfo->hostInfo->dns_mode == DNS_MAX) {
    /* Grrr. Check THAT name to make sure it's really the name of the addr. */
    /* Code from Harald Hanche-Olsen <hanche@imf.unit.no> */
      if(reqInfo->remote_host) {
        char **haddr;

        hptr = gethostbyname(reqInfo->remote_host);
        if (hptr) {
            for(haddr=hptr->h_addr_list;*haddr;haddr++) {
                if(((struct in_addr *)(*haddr))->s_addr == iaddr->s_addr)
                    break;
            }
        }
        if((!hptr) || (!(*haddr)))
	    if (reqInfo->remote_host) {
		freeString(reqInfo->remote_host);
            	reqInfo->remote_host = NULL;
	    }
      }
    }
    reqInfo->remote_ip = dupStringP(inet_ntoa(*iaddr),STR_REQ);
    if(!reqInfo->remote_host){
	if (reqInfo->remote_name) {
	  freeString(reqInfo->remote_name);
        }
        reqInfo->remote_name = dupStringP(reqInfo->remote_ip,STR_REQ);
    }
    if (!reqInfo->remote_name){
	reqInfo->remote_name = dupStringP("UNKNOWN_HOST",STR_REQ);
    }
}

char *get_remote_logname(FILE *fd) {
    int len;
    char *result;
#ifdef MIX_SOCKADDR
    struct sockaddr sa_server, sa_client;
#else
    struct sockaddr_in sa_server,sa_client;
#endif

    len = sizeof(sa_client);
    if(getpeername(fileno(fd),(struct sockaddr *)&sa_client,&len) != -1) {
        len = sizeof(sa_server);
        if(getsockname(fileno(fd),(struct sockaddr *)&sa_server,&len) == -1){
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

    if(!(gConfiguration->httpd_conf & HC_SERVER_HOSTNAME)) {
        struct hostent *p;
        gethostname(str, len);
        if((!(p=gethostbyname(str))) || 
	   (!(gConfiguration->server_hostname = find_fqdn(p)))) {
            fprintf(stderr,"HTTPd: cannot determine local host name.\n");
            fprintf(stderr,"Use ServerName to set it manually.\n");
            exit(1);
        }
    }
}

void get_local_addr(per_request *reqInfo) {
  struct sockaddr addr;
  int len;

  len = sizeof(struct sockaddr);
  if (getsockname(reqInfo->connection_socket, &addr, &len) < 0) {
    char error_msg[100];
    reqInfo->hostInfo = gConfiguration;
    sprintf(error_msg,"get_local_addr: could not get local address, errno is %d",
    		errno);
    die(reqInfo,SC_SERVER_ERROR,error_msg);
  }
  memcpy(&(reqInfo->address_info),&((struct sockaddr_in *)&addr)->sin_addr,
	 sizeof(struct in_addr));
}

/* Modified: Tue Sep  5 23:18:01 1995
 * This function now understands the "https" directive and the 
 * default SSL port of 443.
 */
void construct_url(char *full_url, per_host *host, char *url) 
{
  {
     if (port == DEFAULT_PORT)
       sprintf(full_url,"%s://%s%s", "http",host->server_hostname,url);
      else 
       sprintf(full_url,"%s://%s:%d%s", "http",host->server_hostname,port,url);
   }
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
    int nbytesdecoded;
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
    while(pr2six[(int) *(bufin++)] <= 63);
    nprbytes = bufin - bufcoded - 1;
    nbytesdecoded = ((nprbytes+3)/4) * 3;
    if(nbytesdecoded > outbufsize) {
        nprbytes = (outbufsize*4)/3;
    }
    
    bufin = bufcoded;
    
    while (nprbytes > 0) {
        *(bufout++) = 
            (unsigned char) (pr2six[(int) *bufin] << 2 | pr2six[(int) bufin[1]] >> 4);
        *(bufout++) = 
            (unsigned char) (pr2six[(int) bufin[1]] << 4 | pr2six[(int) bufin[2]] >> 2);
        *(bufout++) = 
            (unsigned char) (pr2six[(int) bufin[2]] << 6 | pr2six[(int) bufin[3]]);
        bufin += 4;
        nprbytes -= 4;
    }
    
    if(nprbytes & 03) {
        if(pr2six[(int) bufin[-2]] > 63)
            nbytesdecoded -= 2;
        else
            nbytesdecoded -= 1;
    }
    bufplain[nbytesdecoded] = '\0';
}
