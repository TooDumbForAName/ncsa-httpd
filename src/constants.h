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
 * constants.h,v 1.54 1996/04/05 18:54:42 blong Exp
 *
 ************************************************************************
 *
 *
 * constants.h 	contains all non-user configurable constants with some
 * 	associated structures
 *
 *  Contains HTTP_TIME_FORMAT
 *		SERVER_VERSION
 *		SERVER_PROTOCAL
 *		errors
 *		methods
 *		object types
 *		security options
 *		magic mime types
 *		directory indexing options
 *		struct KeepAliveData
 *		struct security_data
 *		
 */

#ifndef _CONSTANTS_H_
#define _CONSTANTS_H_

#include <stdio.h>
#include <netinet/in.h>

#define TRUE  1
#define FALSE 0

#define MAX_STRING_LEN 256			
#define HUGE_STRING_LEN 8192
#define IOBUFSIZE 8192				


#ifdef NEXT
# define HTTP_TIME_FORMAT "%a, %d %b %Y %X"
#else
# define HTTP_TIME_FORMAT "%a, %d %b %Y %T GMT" 
#endif /* NEXT */

#define SERVER_VERSION "NCSA/1.5.2"
#define SERVER_SOURCE "NCSA/1.5.2"
#define SERVER_PROTOCOL "HTTP/1.0"

/* Response Codes from HTTP/1.0 Spec 
   all 4 digit codes are internal only */
#define SC_CONTINUE		100
#define SC_SWITCHING_PROTOCOLS	101
#define SC_DOCUMENT_FOLLOWS 	200
#define SC_CREATED		201
#define SC_ACCEPTED		202
#define SC_NON_AUTH_INFO	203
#define SC_NO_CONTENT		204
#define SC_RESET_CONTENT	205
#define SC_PARTIAL_CONTENT	206
#define SC_MULTIPLE_CHOICES	300
#define SC_REDIRECT_PERM	301
#define SC_REDIRECT_TEMP 	302
#define SC_REDIRECT_LOCAL       3020
#define SC_SEE_OTHER		303
#define SC_USE_LOCAL_COPY 	304
#define SC_USE_PROXY		305
#define SC_BAD_REQUEST 		400
#define SC_AUTH_REQUIRED 	401
#define SC_PAY_REQUIRED		402
#define SC_FORBIDDEN 		403
#define SC_NOT_FOUND 		404
#define SC_METHOD_NOT_ALLOWED	405
#define SC_NONE_ACCEPTABLE	406
#define SC_PROXY_AUTH_REQUIRED	407
#define SC_REQUEST_TIMEOUT	408
#define SC_CONFLICT		409
#define SC_GONE			410
#define SC_LENGTH_REQUIRED      411
#define SC_UNLESS_TRUE		412
#define SC_SERVER_ERROR 	500
#define SC_NOT_IMPLEMENTED 	501
#define SC_BAD_GATEWAY		502
#define SC_SERVICE_UNAVAIL	503
#define SC_GATEWAY_TIMEOUT	504
#define SC_NO_MEMORY 	  	6992
#define SC_CONF_ERROR 	  	6993
#define SC_BAD_IMAGEMAP         6994
#define SC_AUTH_NO_WWW_AUTH	7001

/* Supported Methods - sorta */
#define METHODS 		8
#define M_GET 			0 
#define M_HEAD			1
#define M_POST 		 	2
#define M_PUT 			3	
#define M_DELETE 		4
#define M_SECURE		5
#define M_INVALID              -1

/* Unsupported Methods */ 
#define M_LINK			6
#define M_UNLINK		7
#define M_OPTIONS		8
#define M_PATCH			9
#define M_COPY			10
#define M_MOVE			11
#define M_TRACE			12
#define M_WRAPPED		13

/* Array containing Method names */
extern char *methods[];

/* Supported Protocals - sorta */
#define PROTOCALS       6
#define P_OTHER		0
#define P_HTTP_0_9	1
#define P_HTTP_1_0	2
#define P_HTTP_1_1	3
#define P_SHTTP_1_1	4
#define P_SHTTP_1_2	5

extern char *protocals[];

/* Object types */
#define A_STD_DOCUMENT 		  0
#define A_REDIRECT_TEMP 	  1
#define A_REDIRECT_PERM		  2
#define A_SCRIPT_CGI 		  3
#define A_SCRIPT_FCGI		  4

/* Security Options */
#define OPT_NONE 		  0
#define OPT_INDEXES 		  1
#define OPT_INCLUDES 		  2
#define OPT_SYM_LINKS 		  4
#define OPT_EXECCGI 		  8
#define OPT_UNSET  		 16
#define OPT_INCNOEXEC 		 32
#define OPT_SYM_OWNER 		 64
#define OPT_ALL (OPT_INDEXES | OPT_INCLUDES | OPT_SYM_LINKS | OPT_EXECCGI)

#define OR_NONE 		  0
#define OR_LIMIT 		  1
#define OR_OPTIONS 		  2
#define OR_FILEINFO  		  4
#define OR_AUTHCFG 		  8
#define OR_INDEXES  		 16
#define OR_REDIRECT		 32
#define OR_PRIVACY_ENHANCE       64
#define OR_ALL (OR_LIMIT | OR_OPTIONS | OR_FILEINFO | OR_AUTHCFG | OR_INDEXES | OR_REDIRECT | OR_PRIVACY_ENHANCE)

/* Magic MIME Types */
#define CGI_MAGIC_TYPE 		 "application/x-httpd-cgi"
#define FCGI_MAGIC_TYPE  	 "application/x-httpd-fcgi"
#define INCLUDES_MAGIC_TYPE      "text/x-server-parsed-html"
#define IMAGEMAP_MAGIC_TYPE      "text/x-imagemap"
#define BLACKOUT_MAGIC_TYPE 	 "text/x-httpd-black"

/* For directory indexing */
#define BY_PATH 			0
#define BY_TYPE 			1
#define BY_ENCODING 			2

#define FANCY_INDEXING 			1

#define LF 		10		
#define CR 		13

#define DENY_THEN_ALLOW 0		
#define ALLOW_THEN_DENY 1
#define MUTUAL_FAILURE 2

#define DIE_KEEPALIVE   2   /* used to determine if connection should */
#define DIE_NORMAL      1   /* remain up on return from die function */

#define DEFAULT_ALLOW_KEEPALIVE          0    /* default if off */
#define DEFAULT_KEEPALIVE_MAXREQUESTS    5
#define DEFAULT_KEEPALIVE_TIMEOUT       10    /* 10 sec per-request timeout*/

typedef struct {
    int bAllowKeepAlive; /* non-zero if configuration allows,else 0 */
    int bKeepAlive;      /* non-zero if keep-alive on, else 0 */
    int nTimeOut;        /* per-request timeout in seconds */
    int nMaxRequests;    /* max requests per session, 0 for no max */
    int nCurrRequests;   /* # of requests so far */
} KeepAliveData;

#define MAX_SECURITY 50	
#define SATISFY_ALL   0
#define SATISFY_ANY   1

#define AUTHFILETYPE_STANDARD 0
#define AUTHFILETYPE_DBM      1
#define AUTHFILETYPE_NIS      2
#define AUTHFILETYPE_RADIUS   3

typedef struct {			
    char d[MAX_STRING_LEN];
    char opts;
    char override;

    int order[METHODS];
    int bSatisfy;             /* 0 = All, 1 = Any */

    int num_allow[METHODS];
    char *allow[METHODS][MAX_SECURITY];

    int num_auth[METHODS];
    char *auth[METHODS][MAX_SECURITY];

    int num_referer_allow[METHODS];
    char *referer_allow[METHODS][MAX_SECURITY];

    int num_referer_deny[METHODS];
    char *referer_deny[METHODS][MAX_SECURITY];

    int num_deny[METHODS];
    char *deny[METHODS][MAX_SECURITY];

    char *on_deny[METHODS];

    char auth_type[MAX_STRING_LEN];
    char auth_name[MAX_STRING_LEN];
    char auth_pwfile[MAX_STRING_LEN];
    char auth_grpfile[MAX_STRING_LEN];
    int  auth_pwfile_type;
    int  auth_grpfile_type;
#ifdef DIGEST_AUTH
    char auth_digestfile[MAX_STRING_LEN];
    int  auth_digestfile_type;
#endif /* DIGEST_AUTH */

} security_data;


typedef struct _ErrorDoc {
/*  int Type; */
  int DocErrorNum;
  char* DocErrorFile;
} ErrorDoc;

/* DNS Mode - Formerly, this was a compile time option, but it was 
   limited, and this is the feature bloat version anyways. */
#define DNS_NONE	0
#define DNS_MIN		1
#define DNS_STD		2
#define DNS_MAX		3

/* ----------- Our socket buffering routine defines ----------------- */
#define SB_NEW     1
#define SB_READ    2
#define SB_FLUSHED 3
#define SB_ERROR   4
 
typedef struct _sock_buf {
  char buffer[HUGE_STRING_LEN];
  int buf_posn;
  int buf_good;
  int status;
  int sd;
} sock_buf;

/* ------------------- per hostname configuration -------------------- */

/* These #defines are for keeping track of which options are links to 
   defaults, and which are real (for clean up in restart, presumeably) */

#define PH_HTTPD_CONF      1
#define PH_SRM_CONF        2
#define PH_PEM_CONF        3


/* Configurate data structure (for what's configurable per host) 
   #def's from above are used to keep track of what is allocated and
   what is just a pointer. */

typedef struct _per_host {
  /* httpd.conf */
  int httpd_conf;
  char *error_fname;
  char *xfer_fname;
  char *agent_fname;
  char *referer_fname;
  char *referer_ignore;
  char *server_admin;
  char *server_hostname;
  char *srm_confname;
  char *annotation_server;
  int dns_mode;

  int log_opts;
  int files_open;
  FILE *error_log;
  FILE *agent_log;
  FILE *referer_log;
  int xfer_log;

  int virtualhost;
  char *called_hostname;
  struct in_addr address_info;

  /* srm.conf */
  int srm_conf;
  char *user_dir;
  char *index_names;
  char *access_name;
  char *document_root;
  int doc_root_len;
  char *default_type;
  char *default_icon;

  int num_doc_errors;
  ErrorDoc **doc_errors; 
  
  struct _lookup *translations;
  struct _per_host *next;
} per_host;

/* --------- Per request Data Structure ------------- */
 
/* Request Flags */
#define DOING_PGP   1
#define DOING_SHTTP 2
#define DOING_SSL   3
 
typedef struct _per_request {
/* Information about Contents; */
  int ownENV;
  int ownDNS;
  int ownSB;
  int RequestFlags;
 
/* Request Information */
  int status;
  char *status_line;
  long bytes_sent;
 
/* request stuff to be logged */
  int method;
  char url[HUGE_STRING_LEN];
  char args[HUGE_STRING_LEN];
  char path_info[HUGE_STRING_LEN];
  int http_version;
  char inh_agent[HUGE_STRING_LEN];
  char inh_referer[HUGE_STRING_LEN];
  char inh_called_hostname[MAX_STRING_LEN];
  char inh_if_mod_since[MAX_STRING_LEN];
  char inh_auth_line[HUGE_STRING_LEN];
  char inh_content_type[MAX_STRING_LEN];
  int  inh_content_length;
 
/* Internal Info */
  char filename[HUGE_STRING_LEN];
 
/* Outgoing information */
  char outh_location[HUGE_STRING_LEN];
  char outh_last_mod[MAX_STRING_LEN];
  char outh_www_auth[HUGE_STRING_LEN];
  char outh_content_type[MAX_STRING_LEN];
  char outh_content_encoding[MAX_STRING_LEN];
  int  outh_content_length;
  char *outh_cgi;
 
#ifdef CONTENT_MD5
  char *outh_content_md5;
#endif /* CONTENT_MD5 */
 
  char auth_type[MAX_STRING_LEN];
  int dirs_in_alias;
 
  /* Authentication Information */
  char  auth_user[MAX_STRING_LEN];
  char  auth_group[MAX_STRING_LEN];
 
  /* authentication files */
  char* auth_name;
  char* auth_pwfile;
  char* auth_grpfile;
  int   auth_pwfile_type;
  int   auth_grpfile_type;
#ifdef DIGEST_AUTH
  char* auth_digestfile;
  int   auth_digestfile_type;
#endif /* DIGEST_AUTH */
 
  /* Domain Restriction Info */
  int bNotifyDomainRestricted;
  int bSatisfiedDomain;
  int bSatisfiedReferer;
  int dns_host_lookup;
 
  int num_env;
  int max_env;
  char **env;
  int *env_len;
 
/* Client Information */
  char *remote_host;
  char *remote_name;
  char *remote_ip;
/*  char *remote_logname; */
 
/* Server Information */
  int connection_socket;
  int in;
  FILE *out;
  sock_buf *sb;
  sock_buf *cgi_buf;
  per_host *hostInfo;
  struct in_addr address_info;
 
/* Linked List of requests */
  struct _per_request *next;
 
} per_request;
 
#endif /* _CONSTANTS_H_ */
