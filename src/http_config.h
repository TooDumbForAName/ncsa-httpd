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
 * http_config.h,v 1.15 1996/03/27 20:44:01 blong Exp
 *
 ************************************************************************
 *
 */


#ifndef _HTTP_CONFIG_H_
#define _HTTP_CONFIG_H_

/* globals defined in this module */

extern int num_sec;
/* num_sec_config: Number of security directories in access.conf file */
extern int num_sec_config;
extern security_data sec[];

/* Server config globals */
extern int standalone;
extern int port;
extern uid_t user_id;
extern gid_t group_id;
extern int timeout;
extern int do_rfc931;
extern int max_requests;
extern char server_confname[];
extern char server_root[];
extern char core_dir[];
#ifdef SETPROCTITLE
extern char process_name[];
#endif /* SETPROCTITLE */
extern char pid_fname[];
extern char access_confname[];
extern char types_confname[];
extern char local_default_type[];
extern char local_default_icon[];
extern int  log_directory_group_write_ok;
extern int  log_directory_other_write_ok;

#ifndef NO_PASS
extern int max_servers;
extern int start_servers;
#endif /* NO_PASS */
 
extern KeepAliveData keep_alive;

/* function prototypes */
void read_config(FILE *errors);
void process_server_config(per_host *host, FILE *cfg, FILE *errors,int virtual);
void process_resource_config(per_host *host, FILE *open, FILE *errors, int virtual);
void parse_htaccess(per_request *reqInfo, char *dir, char override);

#endif /* _HTTP_CONFIG_H_ */
