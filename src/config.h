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
 * config.h,v 1.16 1995/11/10 02:01:46 blong Exp
 *
 ************************************************************************
 *
 *  config.h
 *	Contains user configurable constants and declarations
 *
 */

#ifndef _CONFIG_H
#define _CONFIG_H	1

/*************************************************************************/
/*   Compile Time Options 
 */

/* To enable changing of the process title to the current request being
   handled, uncomment the following.  Note: Using this will cause a
   performance hit (though maybe not much of one).  This doesn't work
   on all systems, either.  It is known to work under AIX3, SunOS, OSF1,
   FreeBSD, and NetBSD. */

/* #define SETPROCTITLE /* */

/* If you have SETPROCTITLE enabled, and you are a stats fanatic, and your
   server has a few extra clock cycles to spare, defining the following
   will enable an RPM (requests per minute) indicator in the proc title. */

#ifdef SETPROCTITLE
#define TACHOMETER /* */
# ifdef TACHOMETER
#  define MAX_TACHOMETER 30
# endif
#endif

/* To not compile with built in imagemap support, comment out the following.
   Note: It is much faster to use this then the external program, but it
   also makes the program size of httpd larger. */

#define IMAGEMAP_SUPPORT /* */

/* If you want the server to check the execute bit of an HTML file to 
   determine if the file should be parsed, uncomment the following.
   Using this feature will give better performance for files which
   are not parsed without the necessity of using the magic mime type */

/* #define XBITHACK /* */

/* If you would like to ensure that CGI scripts don't mess with the 
   log files (except the error_log file), uncomment the following. */

/* #define SECURE_LOGS /* */

/* If you would like to specify the keyword LOCAL in your access
   configuration file to match local address (ie, those without embedded
   dots), uncomment the following. */
 
/* #define LOCALHACK /* */

/* If you would like to use NIS services for passwords and group information,
   uncomment the following.  NOTE: DO NOT USE THIS ON OPEN NETWORKS.  The 
   security information used in Basic Authentication involves sending the
   password in clear text across the network on every request which requires
   it. */

/* #define NIS_SUPPORT /* */

/* If you have a REALLY heavily loaded system, and you can't afford to
   have a server per request(low memory?), you can compile with this i
   option to make max_servers a hard limit. */

/* #define RESOURCE_LIMIT /* */

/* If your system doesn't support file descriptor passing, or if you
   don't want to use it, defining the following will enable HTTPd to
   mimic the 1.3 Forking server.  This should be defined in the system
   specific information in portability.h, and not here. */

/* #define NO_PASS /* */


/* defines for new muli-child approach
  DEFAULT_START_DAEMON defines how many children start at httpd start
  DEFAULT_MAX_DAEMON defines how many children can start
  */

#define DEFAULT_START_DAEMON    5
#define DEFAULT_MAX_DAEMON      10

/* defines for debugging purposes
   PROFILE to set the server up to profile the code 
   QUANTIFY is a profiler from Pure software
   PURIFY is a memory checker from Pure software
*/

/* #define PROFILE /* */

/* #define QUANTIFY /* */
/* #define PURIFY /* */

/* SHELL_PATH defines where the shell path is */

#define SHELL_PATH "/bin/sh"

/* DEFAULT_PATH defines the default search PATH handed to CGI scripts
   if there isn't one in the environment when HTTPd runs */

#define DEFAULT_PATH "/bin:/usr/bin:/usr/ucb:/usr/bsd:/usr/local/bin:."

/* The following define default values for options which can be over-
   ridden at run time via command-line or configuration files */

#define HTTPD_ROOT "/usr/local/etc/httpd"

#define DOCUMENT_LOCATION "/usr/local/etc/httpd/htdoc"
#define DEFAULT_ADMIN "[no address given]"

#define SERVER_CONFIG_FILE "conf/httpd.conf"
#define RESOURCE_CONFIG_FILE "conf/srm.conf"
#define TYPES_CONFIG_FILE "conf/mime.types"
#define ACCESS_CONFIG_FILE "conf/access.conf"

#define DEFAULT_PORT 80
#define DEFAULT_USER "#-1"
#define DEFAULT_GROUP "#-1"

#define DEFAULT_XFERLOG "logs/access_log"
#define DEFAULT_AGENTLOG "logs/agent_log"
#define DEFAULT_REFERERLOG "logs/referer_log"
#define DEFAULT_ERRORLOG "logs/error_log"
#define DEFAULT_PIDLOG "logs/httpd.pid"

#define DEFAULT_REFERERIGNORE ""

#define DEFAULT_INDEX_NAMES "index.html index.shtml index.cgi"
#define DEFAULT_INDEXING 0
#define DEFAULT_TYPE "text/html"
#define DEFAULT_ACCESS_FNAME ".htaccess"

#define DEFAULT_RFC931 0
#define DEFAULT_USER_DIR "public_html"

#define DEFAULT_TIMEOUT 1200

#endif /* _CONFIG_H */
