/*
 * httpd.h: header for simple (ha! not anymore) http daemon
 */

/* ----------------------------- config file ------------------------------ */

/* Define this to be the default configuration file */
#define CONFIG_FILE "/usr/local/etc/httpd/httpd.conf"

/* -------------- Port number for server running standalone --------------- */

#define HTTPD_PORT 80

/* --------- Default user name and group name running standalone ---------- */
/* --- These may be specified as numbers by placing a # before a number --- */

#define DEFAULT_USER "nobody"
#define DEFAULT_GROUP "#-1"

/* Define if your system calls are BSD-style */
#undef BSD

/* ---------------------------- access logging ---------------------------- */

/* If you want to have server access logging, define this. */
#define LOGGING

#ifdef LOGGING

/* The name of the log file -- this must be writeable by the userid
   the server is run under (usually 'nobody'). */
#define LOGFILE "/usr/local/etc/httpd/access_log"

#endif /* LOGGING */

/* ---------------------------- gopher support ---------------------------- */

/* Define this if you want to support . links and .cap gopherisms */
/* This has other effects, see README.GOPHER for details. */
#undef GROK_GOPHER

#ifdef GROK_GOPHER

/* Set this to the port # of your gopher server */
#define GOPHERPORT 70

/* This is the virtual location of the Gopher data directory.  You should
   establish a mapping from this to your real Gopher data directory in
   httpd.conf, as explained in README.GOPHER. */
#define GOPHER_LOCATION "/gopher-data"
#define GOPHER_LOC_LEN 12

#endif /* GROK_GOPHER */

/* ----------------------- group annotation support ----------------------- */

/* Define this if you want this server to serve as a group annotation
   server.  See README for details. */
#undef ANNOTATIONS
/* #define ANNOTATIONS */

#ifdef ANNOTATIONS

/* The physical directory where all group annotations will be stored.
   THIS MUST BE WRITEABLE by the user ID under which you run httpd
   (usually 'nobody'). */
#define ANN_DIR "/usr/local/etc/httpd/annotations"

/* The virtual directory (i.e., from the outside coming into the server)
   where the group annotations will be located.  

   E.g., if you normally map / to /usr/local/etc/httpd in httpd.conf,
   and you have ANN_DIR set to /usr/local/etc/httpd/annotations, then
   this should be "/annotations" (note the slash).

   This means that the annotation directory must be browsable
   according to the standard rules in httpd.conf. */
#define ANN_VIRTUAL_DIR "/annotations"

/* This is your port number, same number used in /etc/services. */
#define ANN_PORT 80

#endif

/* ------------------------- other customizations ------------------------- */

/* Define this if you want errors logged to syslog and your system has the
   syslog() system call and syslog.h */
#undef SYSLOG

/* The default string lengths */
#define MAX_STRING_LEN 256

/* The maximum number of aliases in the config file */
#define MAX_ALIASES 20

/* Define this to be what your HTML directory content files are called */
#define HTML_DIR_CONTENT "index.html"

/* Set this to the hardcoded fully qualified machine name. 

   You may #undef this if you choose, and httpd will then use the
   gethostname() and gethostbyname() calls to figure out the fully
   qualified machine name on the fly; HOWEVER, gethostbyname() called
   on the name of the local host coredumps on some systems. */
/* #define HARDCODED_HOSTNAME "machine.foo.edu" */
#undef HARDCODED_HOSTNAME

/*
 * The number of characters to truncate the outgoing file's lines to
 * "Well behaved" servers should truncate this to 80. Some link pathnames 
 * tend to go ballistic, so I would set this higher. 
 * 
 * If you want to serve JPEG's and other such binary files, #undef this. */
#undef TRUNCATE_LEN

/*
 * The particular directory style your system supports. If you have dirent.h
 * in /usr/include (POSIX) or /usr/include/sys (SYSV), #include 
 * that file and define DIR_TYPE to be dirent. Otherwise, if you have 
 * /usr/include/sys/dir.h, define DIR_TYPE to be direct and include that
 * file. If you have neither, I'm confused.
 */
#include <dirent.h>
#define DIR_TYPE dirent

/* ------------------------------ error urls ------------------------------ */

/* Error files. BE SURE THESE WORK! NOTE THAT ALIASES ARE NOT TRANSLATED! */
#define SERVER_ERROR "/usr/local/etc/httpd/errors/bad_server.html"
#define ANN_SERVER_ERROR "/usr/local/etc/httpd/errors/bad_ann_server.html"
#define BAD_REQUEST "/usr/local/etc/httpd/errors/bad_request.html"
#define BAD_ANN_REQUEST "/usr/local/etc/httpd/errors/bad_ann_request.html"
#define BAD_FILE "/usr/local/etc/httpd/errors/bad_file.html"
#define UNIMPLEMENTED "/usr/local/etc/httpd/errors/unimplemented.html"
#define GOPHER_ERROR "/usr/local/etc/httpd/errors/gopher.html"

/* ----------------------------- other stuff ------------------------------ */

/* You shouldn't have to edit anything below this line. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/ioctl.h>

#ifdef NEXT_BRAIN_DEATH
#define S_ISDIR(m)      (((m)&(S_IFMT)) == (S_IFDIR))
#define S_ISREG(m)      (((m)&(S_IFMT)) == (S_IFREG))
#endif

#ifdef ultrix
#define ULTRIX_BRAIN_DEATH
#define NEED_STRDUP
#endif

#ifdef SYSLOG
#include <syslog.h>
#endif

/* Just in case your linefeed isn't the one the other end is expecting. */
#define LINEFEED 10
#define CRETURN 13

/* Messages which are logged upon system errors. */
#define CONFIG_FILENAME \
   "httpd: configuration file not found or permission denied"
#define CONFIG_SYNTAX "httpd: syntax error in configuration file"
#define MEMORY "httpd: ran out of memory"
#define FORK "httpd: could not fork"
#define SOCKET "httpd: socket problem"
#define SOCKET_BIND "httpd: socket bind problem"
#define SOCKET_ACCEPT "httpd: connection accept problem"
#define BAD_USERNAME "httpd: bad username"
#define BAD_GROUPNAME "httpd: bad groupname"


#ifdef SYSLOG
#define msg_out(fd,msg) (syslog(LOG_ERR,(msg)))
#else
#if 1
#define msg_out(fd,msg) fprintf((fd),(msg))
#else
#define msg_out(fd,msg) (fprintf((fd),(msg)),send_error_file((msg),(fd)))
#endif
#endif

/* log_error prints a server error to the appropriate places and exits */
#define server_error(fd,msg) (msg_out(fd,msg),send_error_file(msg,fd),\
   exit(0))
#define client_error(fd,msg) (send_error_file(msg,fd),exit(0))


struct ent {
    char *name;
    struct ent *next;
};


typedef struct {
	char *url;
	int acnt;
	int *ann_array;
} LogRec;

typedef struct {
	int hash;
	int cnt;
	LogRec *logs;
} LogData;


/* Function prototypes. */
extern int read_config(char *name, FILE *errors);
extern void send_node(char *name, FILE *fd);
extern void translate_name(char *name);
extern void process_request(FILE *in, FILE *out);
extern void Get_request(FILE *in, FILE *out);
extern void Set_request(FILE *in, FILE *out);
extern void Change_request(FILE *in, FILE *out);
extern void Delete_request(FILE *in, FILE *out);
extern void strsubfirst(int start,char *dest, char *src);
extern void strcpy_nocrlf(char *dest, char *src);
extern char *strsub(char *src, char *oldstr, char *newstr);
extern void make_full_path(char *src1,char *src2,char *dst);
extern int is_directory(char *name);
extern char *full_hostname(void);
extern void index_directory(char *name, FILE *fd);
extern void getparents(char *name);
extern void gopher_index(char *name,FILE *fd);
extern void send_error_file(char *file,FILE *fd);
extern void send_file(char *file,FILE *fd);
extern void send_error_file(char *file,FILE *fd);
extern LogData *GetLogData(int hash);
extern int FindLogData(LogData *lptr, char *url, int **alist, int *acnt);
extern int AddLogData(LogData *lptr, int indx, char *url);
extern char *HashtoUrl(LogData *lptr, int indx);
extern int WriteLogData(LogData *lptr);
extern void FreeLogData(LogData *lptr);
extern char *WriteAudioData(int hash, int indx, char *data, int len, char *type, int no_change);
extern int WriteAnnData(char *url, int hash, int indx, char *title, char *user, char *date, char *data, int len, int no_change);
extern int ReadAnnData(int hash, int indx, char **title, char **user, char **date);
extern int DeleteLogData(LogData *lptr, int indx);
extern void UnlockIt(void);
extern uid_t uname2id(char *name);
extern gid_t gname2id(char *name);

#ifdef NEED_STRDUP
extern char *strdup (char *str);
#endif
