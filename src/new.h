/* 
 *  New Header file, since I didn't like recompiling the whole thing 
 *  every time I wanted to do anything.
 *
 * One of these days, I'll have to make some real header files for this code.
 *
 * All code contained herein is covered by the Copyright as distributed
 * in the README file in the main directory of the distribution of 
 * NCSA HTTPD.
 *
 */

#include <setjmp.h>

/* #defines for new muli-child approach
  DEFAULT_START_DAEMON defines how many children start at httpd start
  DEFAULT_MAX_DAEMON defines how many children can start
  */

#define DEFAULT_START_DAEMON	5
#define DEFAULT_MAX_DAEMON	10

/* For New Error Handling */
#define NUM_ERRORS      10
#define CGI_ERROR	1
#define TEXT_ERROR	2

typedef struct _ChildInfo {
  int parentfd;
  int childfd;
  int pid;
  int busy;
} ChildInfo;

typedef struct _ErrorMessage {
  int Type;
  int ErrorNum;
  char* ErrorFile;
} ErrorMessage;

/* in http_log.c */
extern const char StatLine200[];
extern const char StatLine302[];
extern const char StatLine304[];
extern const char StatLine400[];
extern const char StatLine401[];
extern const char StatLine403[];
extern const char StatLine404[];
extern const char StatLine500[];
extern const char StatLine501[];
extern int ErrorStat;
extern int numErrorsDefined;

/* in http_mime.c */
extern char *status_line;
extern struct mime_ext *types[27];
extern struct mime_ext *forced_types;
extern struct mime_ext *encoding_types;
extern struct mime_ext *Saved_Forced;
extern struct mime_ext *Saved_Encoding;
char* set_stat_line();
void reset_mime_vars();

/* for http_ipc.c */
int pass_fd(int spipefd, int filedes);
int recv_fd(int spipefd);
#ifdef NEED_SPIPE
int s_pipe(int fd[2]);
#endif

/* for http_log.c */
int add_error(char* errornum, char* name);
int have_error(int errornum);
void reset_error();

/* for http_config.c */
extern int max_servers;
extern int start_servers;
/* number of security directives in access config file */
extern int num_sec_config;	

/* for httpd.c */
void speed_hack_libs();
void set_group_privs();
void set_signals();

/* new globals in http_request.c */
extern char method[];
extern char protocal[];
extern char the_request[];
extern char failed_request[];
extern char as_requested[];
extern char url2[];
extern char failed_url[];
extern char args2[];
void initialize_request();

/* for http_access.c */
void reset_security();
