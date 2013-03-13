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
 * util.h,v 1.14 1995/11/28 09:02:22 blong Exp
 *
 ************************************************************************
 *
 */


#ifndef _UTIL_H_
#define _UTIL_H_

#include <netinet/in.h>
#include <time.h>
#include <sys/stat.h>

/* util function prototypes */
void inststr(char *dst[], int argc, char *src);
void initproctitle(char *start, int argc, char **argv, char **envp);
#ifndef HAVE_SETPROCTITLE
void setproctitle(char *title);
#endif /* HAVE_SETPROCTITLE */
void chdir_file(char *file);
void http2cgi(char* h, char *w);
int later_than(struct tm *tms, char *i);
int strcmp_match(char *str, char *exp);
int is_matchexp(char *str);
void strsubfirst(int start,char *dest, char *src);
void add_file_to_dir(char *dir,char *file);
void make_full_path(char *src1,char *src2,char *dst);
int is_directory(char *name);
void getparents(char *name);
void no2slash(char *name);
uid_t uname2id(char *name);
gid_t gname2id(char *name);
int getline(int sd, char *s, int n, int reset, unsigned int timeout);
int eat_ws (FILE* fp);
int cfg_getline(char *s, int n, FILE *f);
void getword(char *word, char *line, char stop);
void splitURL(char *line, char *url, char *args);
void cfg_getword(char *word, char *line);
int get_remote_host_min(per_request *reqInfo);
void get_remote_host(per_request *reqInfo);
char *get_time(void);
char *gm_timestr_822(time_t t);
char *ht_time(time_t t, char *fmt, int gmt);
struct tm *get_gmtoff(long *tz);
void make_dirstr(char *s, int n, char *d);
int count_dirs(char *path);
void strcpy_dir(char *d, char *s);
void strncpy_dir(char *d, char *s, int n);
void lim_strcpy(char *d, char *s, int n);
void unescape_url(char *url);
void escape_url(char *url);
void escape_uri(char *url);
void escape_shell_cmd(char *cmd);
void plustospace(char *str);
void spacetoplus(char *str);
void str_tolower(char *str);
void uudecode(char *s,unsigned char *d,int dl);
int is_url(char *u);

#ifdef NEED_STRDUP
char *strdup (char *str);
#endif /* NEED_STRDUP */

#ifdef NEED_STRCASECMP
int strcasecmp(const char *s1, const char *s2);
#endif /* NEED_STRCASECMP */

#ifdef NEED_STRNCASECMP
int strncasecmp(const char *s1, const char *s2, int n);
#endif /* NEED_STRNCASECMP */


int ind(char *s, char c);
int rind(char *s, char c);
void construct_url(char *d, per_host *host, char *s);
void get_local_host(void);
void get_local_addr(per_request *reqInfo);
int get_portnum(per_request *reqInfo, int sd);
int can_exec(struct stat *finfo);

#ifdef NEED_INITGROUPS
int initgroups(const char *name, gid_t basegid);
#endif /* NEED_INITGROUPS */

char *get_remote_logname(FILE *fd);
char *rfc931(struct sockaddr_in *rmt_sin,struct sockaddr_in *our_sin);

#endif /* _UTIL_H_ */
