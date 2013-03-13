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
 * http_config.c: auxillary functions for reading httpd's config file
 * and converting filenames into a namespace
 *
 */


#include "config.h"
#include "portability.h"

#include <stdio.h>
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
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "constants.h"
#include "fdwrap.h"
#include "http_config.h"
#include "host_config.h"
#include "http_mime.h"
#include "http_access.h"
#include "http_alias.h"
#include "http_log.h"
#include "http_dir.h"
#include "util.h"
#ifdef FCGI_SUPPORT
# include "fcgi.h"    /* for AppClassCmd() */
#endif /* FCGI_SUPPORT */



/* Server config globals */
int standalone;
int port;
uid_t user_id;
gid_t group_id;
int max_requests;
char server_confname[MAX_STRING_LEN];
int timeout;  
int do_rfc931;
char server_root[MAX_STRING_LEN];
char core_dir[MAX_STRING_LEN];
char pid_fname[MAX_STRING_LEN];
#ifdef SETPROCTITLE
char process_name[MAX_STRING_LEN];
#endif /* SETPROCTITLE */
char access_confname[MAX_STRING_LEN];
char types_confname[MAX_STRING_LEN];
char local_default_type[MAX_STRING_LEN];
char local_default_icon[MAX_STRING_LEN];
int  log_directory_group_write_ok = 0;
int  log_directory_other_write_ok = 0;

/* Access Globals*/
int num_sec;
/* number of security directories in access config file */
int num_sec_config;
security_data sec[MAX_SECURITY];

#ifndef NO_PASS
int max_servers;
int start_servers;
#endif /* NO_PASS */

static int ConfigErrorCritical=TRUE;

void config_error(char *error_msg, char *filename, int lineno, 
			 FILE *errors) 
{
  fprintf(errors,"Syntax error on line %d of %s:\n", lineno, filename);
  fprintf(errors,"%s.\n",error_msg);
  if (ConfigErrorCritical) exit(1);
}

void config_warn(char *warn_msg, char *filename, int lineno, 
		 FILE *errors) 
{
  fprintf(errors,"Syntax warning on line %d of %s:\n", lineno, filename);
  fprintf(errors,"%s.\n",warn_msg);
}

void set_defaults(per_host *host, FILE *errors) 
{
  char tmp[MAX_STRING_LEN];

  standalone = 1;
  port = DEFAULT_PORT;
  user_id = uname2id(DEFAULT_USER);
  group_id = gname2id(DEFAULT_GROUP);
  
#ifdef SETPROCTITLE
  strcpy(process_name, "HTTPd");
#endif /* SETPROCTITLE */

#ifndef NO_PASS
  max_servers = DEFAULT_MAX_DAEMON;
  start_servers = DEFAULT_START_DAEMON;
#endif /* NO_PASS */
  max_requests = DEFAULT_MAX_REQUESTS;

  /* ServerRoot set in httpd.c */

  host->address_info.s_addr = htonl(INADDR_ANY);

  set_host_conf_value(host,PH_HTTPD_CONF,HC_LOG_TYPE);
  host->log_opts = LOG_NONE;

  make_full_path(server_root,DEFAULT_ERRORLOG,tmp);
  set_host_conf(host,PH_HTTPD_CONF,HC_ERROR_FNAME,tmp);

  make_full_path(server_root,DEFAULT_XFERLOG,tmp);
  set_host_conf(host,PH_HTTPD_CONF,HC_XFER_FNAME,tmp);
  strcpy(core_dir,server_root);
  
  make_full_path(server_root,DEFAULT_AGENTLOG,tmp);
  set_host_conf(host,PH_HTTPD_CONF,HC_AGENT_FNAME,tmp);
    
  make_full_path(server_root,DEFAULT_REFERERLOG,tmp);
  set_host_conf(host,PH_HTTPD_CONF,HC_REFERER_FNAME,tmp);

  make_full_path(server_root,RESOURCE_CONFIG_FILE,tmp);
  set_host_conf(host,PH_HTTPD_CONF,HC_SRM_CONFNAME,tmp);

  set_host_conf(host,PH_HTTPD_CONF,HC_REFERER_IGNORE,DEFAULT_REFERERIGNORE);
  set_host_conf(host,PH_HTTPD_CONF,HC_SERVER_ADMIN,DEFAULT_ADMIN);

  make_full_path(server_root,ACCESS_CONFIG_FILE,access_confname);
  make_full_path(server_root,TYPES_CONFIG_FILE,types_confname);
  make_full_path(server_root,DEFAULT_PIDLOG,pid_fname);

  tmp[0] = '\0';
  set_host_conf(host,PH_HTTPD_CONF,HC_ANNOT_SERVER,tmp);
  host->dns_mode = DNS_STD;

  /* initialize keep-alive data to defaults */
  keep_alive.bAllowKeepAlive = DEFAULT_ALLOW_KEEPALIVE;
  keep_alive.nMaxRequests = DEFAULT_KEEPALIVE_MAXREQUESTS;
  keep_alive.nTimeOut = DEFAULT_KEEPALIVE_TIMEOUT;

  timeout = DEFAULT_TIMEOUT;
  do_rfc931 = DEFAULT_RFC931;

  /* default resource config stuff */
  set_host_conf(host,PH_SRM_CONF,SRM_USER_DIR,DEFAULT_USER_DIR);
  set_host_conf(host,PH_SRM_CONF,SRM_INDEX_NAMES,DEFAULT_INDEX_NAMES);
  set_host_conf(host,PH_SRM_CONF,SRM_ACCESS_NAME,DEFAULT_ACCESS_FNAME);
  set_host_conf(host,PH_SRM_CONF,SRM_DOCUMENT_ROOT,DOCUMENT_LOCATION);
  set_host_conf(host,PH_SRM_CONF,SRM_DEFAULT_TYPE,DEFAULT_TYPE);
  set_host_conf(host,PH_SRM_CONF,SRM_DEFAULT_ICON,"");
}



void process_server_config(per_host *host, FILE *cfg, FILE *errors, 
			   int virtual) 
{
  char l[MAX_STRING_LEN],w[MAX_STRING_LEN];
  char tmp[MAX_STRING_LEN];
  static int n;
  int doneSRM = FALSE;
  
  if (!virtual) n=0;
  
  /* Parse server config file. Remind me to learn yacc. */
  while(!(cfg_getline(l,MAX_STRING_LEN,cfg))) {
    ++n;
    if((l[0] != '#') && (l[0] != '\0')) {
      cfg_getword(w,l);
      
      if(!strcasecmp(w,"ServerType") && !virtual) {
	if(!strcasecmp(l,"inetd")) standalone=0;
	else if(!strcasecmp(l,"standalone")) standalone=1;
	else config_error("ServerType is either inetd or standalone",
		    server_confname,n,errors);
      }
      else if(!strcasecmp(w,"LogDirGroupWriteOk")) {
	if(virtual) {
	  config_warn("LogDirGroupWriteOk directive in <VirtualHost> ignored",
		      server_confname,n,errors);
	}
	else {
	    log_directory_group_write_ok = 1;
	}
      }
      else if(!strcasecmp(w,"LogDirOtherWriteOk")) {
	if(virtual) {
	  config_warn("LogDirOtherWriteOk directive in <VirtualHost> ignored",
		      server_confname,n,errors);
	}
	else {
	    log_directory_other_write_ok = 1;
	}
      }
      else if(!strcasecmp(w,"CoreDirectory")) {
        cfg_getword(w,l);
	if (w[0] != '/')
	  make_full_path(server_root,w,l);
         else strcpy(l,w);
	if(!is_directory(l)) {
	  sprintf(tmp,"%s is not a valid directory",l);
	  config_error(tmp,server_confname,n,errors);
	}
	strcpy(core_dir,l);
      }
      else if(!strcasecmp(w,"Port") && !virtual) {
	cfg_getword(w,l);
	port = atoi(w);
      }
      else if(!strcasecmp(w,"BindAddress") && !virtual) {
	struct hostent *hep;
	unsigned long ina;
	cfg_getword(w,l);
	if (!strcmp(w,"*")) {
	  host->address_info.s_addr = htonl(INADDR_ANY);
	} else {
	  hep = gethostbyname(w);
	  if (hep && hep->h_addrtype == AF_INET &&
	      hep->h_addr_list[0] && !hep->h_addr_list[1])
	    {
	      memcpy(&(host->address_info),hep->h_addr_list[0],
		     sizeof(struct in_addr));
	    } else if (!hep && (ina = inet_addr(w)) != -1) {
	      host->address_info.s_addr = ina;
	    } else {
	      config_error("BindAddress must be * or a numeric IP or a name that maps to exactly one address",server_confname,n,errors);
	    }
	}
      }
#ifdef SETPROCTITLE
      else if(!strcasecmp(w,"ProcessName") && !virtual) {
	cfg_getword(w,l);
	strcpy(process_name,w);
      }
#endif /* SETPROCTITLE */
      else if(!strcasecmp(w,"User") && !virtual) {
	cfg_getword(w,l);
	user_id = uname2id(w);
      } 
      else if(!strcasecmp(w,"Group")) {
	cfg_getword(w,l);
	group_id = gname2id(w);
      }
      else if(!strcasecmp(w,"ServerAdmin")) {
	cfg_getword(w,l);
	set_host_conf(host,PH_HTTPD_CONF,HC_SERVER_ADMIN,w);
      }
      /* SSG-4/4/95 read annotation server directive */
      else if(!strcasecmp(w,"Annotation-Server")) {
	cfg_getword(w,l);
	set_host_conf(host,PH_HTTPD_CONF,HC_ANNOT_SERVER,w);
      } 
      else if(!strcasecmp(w,"ServerName")) {
	cfg_getword(w,l);
	set_host_conf(host,PH_HTTPD_CONF,HC_SERVER_HOSTNAME,w);
      }
      else if(!strcasecmp(w,"ServerRoot")) {
	cfg_getword(w,l);
	if(!is_directory(w)) {
	  sprintf(tmp,"%s is not a valid directory",w);
	  config_error(tmp,server_confname,n,errors);
	}
	strcpy(server_root,w);
	strcpy(core_dir,w);
	make_full_path(server_root,DEFAULT_ERRORLOG,tmp);
	set_host_conf(host,PH_HTTPD_CONF,HC_ERROR_FNAME,tmp);
	make_full_path(server_root,DEFAULT_XFERLOG,tmp);
	set_host_conf(host,PH_HTTPD_CONF,HC_XFER_FNAME,tmp);
	make_full_path(server_root,DEFAULT_AGENTLOG,tmp);
	set_host_conf(host,PH_HTTPD_CONF,HC_AGENT_FNAME,tmp);
	make_full_path(server_root,DEFAULT_REFERERLOG,tmp);
	set_host_conf(host,PH_HTTPD_CONF,HC_REFERER_FNAME,tmp);
	make_full_path(server_root,RESOURCE_CONFIG_FILE,tmp);
	set_host_conf(host,PH_HTTPD_CONF,HC_SRM_CONFNAME,tmp);

	if (!virtual) {
	  make_full_path(server_root,DEFAULT_PIDLOG,pid_fname);
	  make_full_path(server_root,ACCESS_CONFIG_FILE,access_confname);
	  make_full_path(server_root,TYPES_CONFIG_FILE,types_confname);
	}
      }
      else if(!strcasecmp(w,"ErrorLog")) {
	cfg_getword(w,l);
	if(w[0] != '/')
	  make_full_path(server_root,w,tmp);
	else strcpy(tmp,w);
	set_host_conf(host,PH_HTTPD_CONF,HC_ERROR_FNAME,tmp);
      } 
      else if(!strcasecmp(w,"TransferLog")) {
	cfg_getword(w,l);
	if(w[0] != '/')
	  make_full_path(server_root,w,tmp);
	else strcpy(tmp,w);
	set_host_conf(host,PH_HTTPD_CONF,HC_XFER_FNAME,tmp);
      }
      else if(!strcasecmp(w,"AgentLog")) {
	cfg_getword(w,l);
	if(w[0] != '/')
	  make_full_path(server_root,w,tmp);
	else strcpy(tmp,w);
	set_host_conf(host,PH_HTTPD_CONF,HC_AGENT_FNAME,tmp);
      }
      else if(!strcasecmp(w,"RefererLog")) {
	cfg_getword(w,l);
	if(w[0] != '/')
	  make_full_path(server_root,w,tmp);
	else strcpy(tmp,w);
	set_host_conf(host,PH_HTTPD_CONF,HC_REFERER_FNAME,tmp);
      }
      else if(!strcasecmp(w,"RefererIgnore")) {
	  /*cfg_getword(w,l);*/
	  sprintf (tmp, " %s ", l);
	  /*strcpy(tmp,w);*/
	  set_host_conf(host,PH_HTTPD_CONF,HC_REFERER_IGNORE,tmp);
      }
      else if(!strcasecmp(w,"PidFile")) {
	cfg_getword(w,l);
	if(w[0] != '/')
	  make_full_path(server_root,w,pid_fname);
	else strcpy(pid_fname,w);
      }
      else if(!strcasecmp(w,"DNSMode")) {
        if(!strcasecmp(l,"Maximum")) host->dns_mode = DNS_MAX;
	else if(!strcasecmp(l,"Standard")) host->dns_mode = DNS_STD;
        else if(!strcasecmp(l,"Minimum")) host->dns_mode = DNS_MIN;
	else if(!strcasecmp(l,"None")) host->dns_mode = DNS_NONE;
        else 
	  config_error("DNSMode is either Maximum, Standard, Minimum, or None",
                    server_confname,n,errors);
      }
      else if(!strcasecmp(w,"LogOptions")) {
	while (l[0]) {
  	  cfg_getword(w,l);
	  if (!strcasecmp(w,"None")) {
	    set_host_conf_value(host,PH_HTTPD_CONF,HC_LOG_TYPE);
	    host->log_opts &= LOG_NONE;
	  }
	  if (!strcasecmp(w,"Combined")) {
	    set_host_conf_value(host,PH_HTTPD_CONF,HC_LOG_TYPE);
	    host->log_opts |= LOG_COMBINED;
          } else if (!strcasecmp(w,"Separate")) {
	    set_host_conf_value(host,PH_HTTPD_CONF,HC_LOG_TYPE);
	    host->log_opts &= LOG_SEPARATE;
          } else if (!strcasecmp(w,"ServerName")) {
            set_host_conf_value(host,PH_HTTPD_CONF,HC_LOG_TYPE);
            host->log_opts |= LOG_SERVERNAME;
	  } else if (!strcasecmp(w,"Date")) {
	    set_host_conf_value(host,PH_HTTPD_CONF,HC_LOG_TYPE);
	    host->log_opts |= LOG_DATE;
	  } else {
	    config_error("Valid LogOptions are Combined or Separate, ServerName",
			  server_confname,n,errors);	
          }
	}
      }
      else if(!strcasecmp(w,"AccessConfig") && !virtual) {
	cfg_getword(w,l);
	if(w[0] != '/')
	  make_full_path(server_root,w,access_confname);
	else strcpy(access_confname,w);
      }
      else if(!strcasecmp(w,"ResourceConfig")) {
	cfg_getword(w,l);
	if(w[0] != '/')
	  make_full_path(server_root,w,tmp);
	else strcpy(tmp,w);
	set_host_conf(host,PH_HTTPD_CONF,HC_SRM_CONFNAME,tmp);
	if (!virtual) doneSRM = TRUE;
	process_resource_config(host,NULL,errors,virtual);
      }
      else if(!strcasecmp(w,"TypesConfig")) {
	cfg_getword(w,l);
	if(w[0] != '/')
	  make_full_path(server_root,w,types_confname);
	else strcpy(types_confname,w);
      }
      else if(!strcasecmp(w,"DocumentRoot")) {
        cfg_getword(w,l);
        if(!is_directory(w)) {
          sprintf(tmp,"%s is not a valid directory",w);
          config_error(tmp,server_confname,n,errors);
        }
        /* strip ending / for backward compatibility */
        if ((strlen(w) > 1) && w[strlen(w) - 1] == '/') w[strlen(w) - 1] = '\0';
        set_host_conf(host,PH_SRM_CONF,SRM_DOCUMENT_ROOT,w);
      }
      else if(!strcasecmp(w,"Timeout"))
	timeout = atoi(l);
      else if(!strcasecmp(w,"IdentityCheck")) {
	cfg_getword(w,l);
	if(!strcasecmp(w,"on"))
	  do_rfc931 = TRUE;
	else if(!strcasecmp(w,"off"))
	  do_rfc931 = FALSE;
	else {
	  config_warn("IdentityCheck must be on or off",
		      server_confname,n,errors);
	}
      }
      else if(!strcasecmp(w,"KeepAlive")) {
	cfg_getword(w,l);
	if(!strcasecmp(w,"on"))
	  keep_alive.bAllowKeepAlive = 1;
	else if(!strcasecmp(w,"off"))
	  keep_alive.bAllowKeepAlive = 0;
	else {
	  config_warn("KeepAlive must be on or off",
		      server_confname,n,errors);
	}
      }
      else if(!strcasecmp(w,"KeepAliveTimeout")) {
	cfg_getword(w,l);
	keep_alive.nTimeOut = atoi(w);
      }
      else if(!strcasecmp(w,"MaxKeepAliveRequests")) {
	cfg_getword(w,l);
	keep_alive.nMaxRequests = atoi(w);
      }
      else if(!strcasecmp(w,"MaxServers")) {
#ifndef NO_PASS
	max_servers = atoi(l);
#else
	config_warn("MaxServers unsupported with NO_PASS compile",
		    server_confname,n,errors);
#endif /* NO_PASS */
      }
      else if(!strcasecmp(w,"StartServers")) {
#ifndef NO_PASS
	start_servers = atoi(l);
#else		
	config_warn("StartServers unsupported with NO_PASS compile",
		    server_confname,n,errors);
#endif /* NO_PASS */
      }
      else if(!strcasecmp(w,"MaxRequestsPerChild")) {
	max_requests = atoi(l);
      }
#ifdef DIGEST_AUTH
      else if(!strcasecmp(w,"AssumeDigestSupport")) {
      /* Doesn't do anything anymore, but if we take it out, anyone with
       * it in their configuration files would complain.  *sigh* 
       */
      }
#endif /* DIGEST_AUTH */
      else if(((!strcasecmp(w,"<VirtualHost")) || (!strcasecmp(w,"<Host")))
	      && !virtual) {
	struct hostent *hep;
	unsigned long ina;
	per_host *newHost;
	int virtualhost = FALSE;
	char name[MAX_STRING_LEN];
	
	if (!strcasecmp(w,"<VirtualHost")) virtualhost = TRUE;

	if (!doneSRM) {
	  process_resource_config(host,NULL,errors,FALSE);
	  doneSRM = TRUE;
	}
	newHost = create_host_conf(host,virtualhost);
	getword(w,l,'>');
	getword(name,w,' ');
	if (!strcasecmp(w,"Optional")) {
	  ConfigErrorCritical = FALSE;
        } else if (!strcasecmp(w,"Required")) {
	  ConfigErrorCritical = TRUE;
   	}
	hep = gethostbyname(name);
	if (hep && hep->h_addrtype == AF_INET &&
	    hep->h_addr_list[0] && !hep->h_addr_list[1])
	{
	    memcpy(&newHost->address_info, hep->h_addr_list[0],
		   sizeof(struct in_addr));
        } else if (!hep && (ina = inet_addr(name)) != -1) {
	    newHost->address_info.s_addr = ina;
	} else {
	    config_error("Argument for VirtualHost must be a numeric IP address, or a name that maps to exactly one address",server_confname,n,errors);
	}
	if (!virtualhost) newHost->called_hostname = strdup(name);
	process_server_config(newHost,cfg,errors,TRUE);
      }
      else if(((!strcasecmp(w,"</VirtualHost>")) || (!strcasecmp(w,"</Host>")))
               && virtual) {
	ConfigErrorCritical = 1;
	return;
     }
     else if(!strcasecmp(w,"<SRMOptions>")) {
       process_resource_config(host,cfg,errors,virtual);
     }
     else {
	sprintf(tmp,"Unknown keyword %s",w);
	config_error(tmp,server_confname,n,errors);
      }
    }
  }
  if (!doneSRM) process_resource_config(host,NULL,errors,FALSE);
}

void process_resource_config(per_host *host, FILE *open, FILE *errors, 
			     int virtual) 
{
  FILE *cfg;
  char l[MAX_STRING_LEN],w[MAX_STRING_LEN];
  char w2[MAX_STRING_LEN];
  char tmp[MAX_STRING_LEN];
  int n=0;
  per_request fakeit;
  
  fakeit.out = errors;
    
  if (!virtual) add_opts_int(&fakeit,FI_GLOBAL,0,"/");
 
  if (open == NULL) {
    if(!(cfg = fopen(host->srm_confname,"r"))) {
      fprintf(errors,"HTTPd: could not open document config file %s\n",
	      host->srm_confname);
      perror("fopen");
      if (ConfigErrorCritical) exit(1);
	else return;
    }
  } else cfg = open;
  while(!(cfg_getline(l,MAX_STRING_LEN,cfg))) {
    ++n;
    if((l[0] != '#') && (l[0] != '\0')) {
      cfg_getword(w,l);
      if(!strcasecmp(w,"ScriptAlias")) {
	cfg_getword(w,l);
	cfg_getword(w2,l);
	if((w[0] == '\0') || (w2[0] == '\0')) {
	  config_error(
"ScriptAlias must be followed by a fakename, one space, then a realname",
host->srm_confname,n,errors);
	}                
	if (!set_host_conf_value(host,PH_SRM_CONF,SRM_TRANSLATIONS)) {
	  host->translations = NULL;
	}
	add_alias(host,w,w2,A_SCRIPT_CGI);
      }
      else if(!strcasecmp(w,"OldScriptAlias")) {
	config_warn("OldScriptAlias directive obsolete, ignored",
		    host->srm_confname,n,errors);
      }
#ifdef FCGI_SUPPORT
      else if(!strcasecmp(w,"FCGIScriptAlias")) {
        cfg_getword(w,l);
        cfg_getword(w2,l);
        if((w[0] == '\0') || (w2[0] == '\0')) {
          config_error(
"FCGIScriptAlias must be followed by a fakename, one space, then a realname",
host->srm_confname,n,errors);
        }
        if (!set_host_conf_value(host,PH_SRM_CONF,SRM_TRANSLATIONS)) {
          host->translations = NULL;
        }
        add_alias(host,w,w2,A_SCRIPT_FCGI);
      }
      else if(!strcasecmp(w,"AppClass")) {
        char *result;
        if (!set_host_conf_value(host,PH_SRM_CONF,SRM_TRANSLATIONS)) {
          host->translations = NULL;
        }
        result = AppClassCmd(host, l);
        if (result)
          config_error(result, host->srm_confname, n, errors);
      }
#endif /* FCGI_SUPPORT */
      else if(!strcasecmp(w,"UserDir")) {
	cfg_getword(w,l);
	if(!strcmp(w,"DISABLED"))
	  tmp[0] = '\0';
	else
	  strcpy(tmp,w);
	set_host_conf(host,PH_SRM_CONF,SRM_USER_DIR,tmp);
      }
      else if(!strcasecmp(w,"DirectoryIndex")) {
	set_host_conf(host,PH_SRM_CONF,SRM_INDEX_NAMES,l);
      } 
      else if(!strcasecmp(w,"DefaultType")) {
	cfg_getword(w,l);
	set_host_conf(host,PH_SRM_CONF,SRM_DEFAULT_TYPE,w);
      }
      else if(!strcasecmp(w,"AccessFileName")) {
	cfg_getword(w,l);
	set_host_conf(host,PH_SRM_CONF,SRM_ACCESS_NAME,w);
      } 
      else if(!strcasecmp(w,"DocumentRoot")) {
	cfg_getword(w,l);
	if(!is_directory(w)) {
	  sprintf(tmp,"%s is not a valid directory",w);
	  config_error(tmp,host->srm_confname,n,errors);
	}
	/* strip ending / for backward compatibility */
	if (w[strlen(w) - 1] == '/') w[strlen(w) - 1] = '\0';
	set_host_conf(host,PH_SRM_CONF,SRM_DOCUMENT_ROOT,w);
      } 
      else if(!strcasecmp(w,"Alias")) {
	cfg_getword(w,l);
	cfg_getword(w2,l);
	if((w[0] == '\0') || (w2[0] == '\0')) {
	  config_error(
"Alias must be followed by a fakename, one space, then a realname",
host->srm_confname,n,errors);
	}                
	if (!set_host_conf_value(host,PH_SRM_CONF,SRM_TRANSLATIONS)) {
	  host->translations = NULL;
	}
	add_alias(host,w,w2,A_STD_DOCUMENT);
      }
      else if(!strcasecmp(w,"AddType")) {
	cfg_getword(w,l);
	cfg_getword(w2,l);
	if((w[0] == '\0') || (w2[0] == '\0')) {
	  config_error(
"AddType must be followed by a type, one space, then a file or extension",
host->srm_confname,n,errors);
	}
	add_type(&fakeit,w2,w);
      }
      else if(!strcasecmp(w,"AddEncoding")) {
	cfg_getword(w,l);
	cfg_getword(w2,l);
	if((w[0] == '\0') || (w2[0] == '\0')) {
	  config_error(
"AddEncoding must be followed by a type, one space, then a file or extension",
host->srm_confname,n,errors);
	}
	add_encoding(&fakeit,w2,w);
      }
      else if(!strcasecmp(w,"Redirect") || !strcasecmp(w,"RedirectTemp")) {
	cfg_getword(w,l);
	cfg_getword(w2,l);
	if((w[0] == '\0') || (w2[0] == '\0') || 
	   !(is_url(w2) || (w2[0] == '/'))) 
	{
	  config_error(
"Redirect must be followed by a document, one space, then a URL",
host->srm_confname,n,errors);
	}
	if (w2[0] == '/') {
	  char w3[MAX_STRING_LEN];
	  construct_url(w3,host,w2);
	  strcpy(w2,w3);
        }
	if (!set_host_conf_value(host,PH_SRM_CONF,SRM_TRANSLATIONS)) {
	  host->translations = NULL;
	}
	add_redirect(host,w,w2,A_REDIRECT_TEMP);
      }
      else if(!strcasecmp(w,"RedirectPermanent")) {
	cfg_getword(w,l);
	cfg_getword(w2,l);
	if((w[0] == '\0') || (w2[0] == '\0') || 
           !(is_url(w2) || (w2[0] == '/'))) 
	{
	  config_error(
"RedirectPermanent must be followed by a document, one space, then a URL",
host->srm_confname,n,errors);
	}
	if (!set_host_conf_value(host,PH_SRM_CONF,SRM_TRANSLATIONS)) {
	  host->translations = NULL;
	}
	if (w2[0] == '/') {
          char w3[MAX_STRING_LEN];
          construct_url(w3,host,w2);
          strcpy(w2,w3);
        }
	add_redirect(host,w,w2,A_REDIRECT_PERM);
      }
      else if(!strcasecmp(w,"FancyIndexing")) {
	cfg_getword(w,l);
	if(!strcasecmp(w,"on"))
	  add_opts_int(&fakeit,FI_GLOBAL,FANCY_INDEXING,"/");
	else if(!strcasecmp(w,"off"))
	  add_opts_int(&fakeit,FI_GLOBAL,0,"/");
	else {
	  config_error("FancyIndexing must be on or off",
		       host->srm_confname,n,errors);
	}
      }
      else if(!strcasecmp(w,"AddDescription")) {
	char desc[MAX_STRING_LEN];
	int fq;
	if((fq = ind(l,'\"')) == -1) {
	  config_error(
"AddDescription must have quotes around the description",
host->srm_confname,n,errors);
	}
	else {
	  getword(desc,&l[++fq],'\"');
	  cfg_getword(w,&l[fq]);
	  add_desc(&fakeit,FI_GLOBAL,BY_PATH,desc,w,"/");
	}
      }
      else if(!strcasecmp(w,"IndexIgnore")) {
	while(l[0]) {
	  cfg_getword(w,l);
	  add_ignore(&fakeit,FI_GLOBAL,w,"/");
	}
      }
      else if(!strcasecmp(w,"AddIcon")) {
	cfg_getword(w2,l);
	while(l[0]) {
	  cfg_getword(w,l);
	  add_icon(&fakeit,FI_GLOBAL,BY_PATH,w2,w,"/");
	}
      }
      else if(!strcasecmp(w,"AddIconByType")) {
	cfg_getword(w2,l);
	while(l[0]) {
	  cfg_getword(w,l);
	  add_icon(&fakeit,FI_GLOBAL,BY_TYPE,w2,w,"/");
	}
      }
      else if(!strcasecmp(w,"AddIconByEncoding")) {
	cfg_getword(w2,l);
	while(l[0]) {
	  cfg_getword(w,l);
	  add_icon(&fakeit,FI_GLOBAL,BY_ENCODING,w2,w,"/");
	}
      }
      else if(!strcasecmp(w,"AddAlt")) {
	cfg_getword(w2,l);
	while(l[0]) {
	  cfg_getword(w,l);
	  add_alt(&fakeit,FI_GLOBAL,BY_PATH,w2,w,"/");
	}
      }
      else if(!strcasecmp(w,"AddAltByType")) {
	cfg_getword(w2,l);
	while(l[0]) {
	  cfg_getword(w,l);
	  add_alt(&fakeit,FI_GLOBAL,BY_TYPE,w2,w,"/");
	}
      }
      else if(!strcasecmp(w,"AddAltByEncoding")) {
	cfg_getword(w2,l);
	while(l[0]) {
	  cfg_getword(w,l);
	  add_alt(&fakeit,FI_GLOBAL,BY_ENCODING,w2,w,"/");
	}
      }
      else if(!strcasecmp(w,"DefaultIcon")) {
	cfg_getword(w,l);
	set_host_conf(host,PH_SRM_CONF,SRM_DEFAULT_ICON,w);
      }
      else if(!strcasecmp(w,"ReadmeName")) {
	cfg_getword(w,l);
	add_readme(&fakeit,FI_GLOBAL,w,"/");
      }
      else if(!strcasecmp(w,"HeaderName")) {
	cfg_getword(w,l);
	add_header(&fakeit,FI_GLOBAL,w,"/");
      }
      else if(!strcasecmp(w,"IndexOptions"))
	add_opts(&fakeit,FI_GLOBAL,l,"/");
      else if (!strcasecmp(w,"ErrorDocument")) {
        if (!set_host_conf_value(host,PH_SRM_CONF,SRM_DOCERRORS)) {
	  host->doc_errors = NULL;
	  host->num_doc_errors = 0;
	}
	cfg_getword(w,l); /* Get errornum */
	cfg_getword(w2,l); /* Get filename */

	add_doc_error(host,w,w2);
      }
      else if (!strcasecmp(w,"</SRMOptions>")) {
	Saved_Forced = forced_types;
        Saved_Encoding = encoding_types;
	return;
      }
      else {
	sprintf(tmp,"Unknown keyword %s",w);
	config_error(tmp,host->srm_confname,n,errors);
      }
    }
  }
  fclose(cfg);
  
  Saved_Forced = forced_types;
  Saved_Encoding = encoding_types;

}

void access_syntax_error(per_request *reqInfo, int n, char *err, FILE *fp, 
			 char *file) 
{
    if(!file) {
        fprintf(reqInfo->out,
		"Syntax error on line %d of access config file.\n",n);
        fprintf(reqInfo->out,"%s\n",err);
	fclose(fp);
        exit(1);
    }
    else {
        char e[MAX_STRING_LEN];
	FClose(fp);
        sprintf(e,"HTTPd: syntax error or override violation in access control file %s, reason: %s",file,err);
        die(reqInfo,SC_SERVER_ERROR,e);
    }
}

int parse_access_dir(per_request *reqInfo, FILE *f, int line, char or, 
		     char *dir, char *file, int local) 
{
    char l[MAX_STRING_LEN];
    char w[MAX_STRING_LEN];
    char w2[MAX_STRING_LEN];
    int n=line;
    register int x,i;

    x = num_sec;
    if (num_sec > MAX_SECURITY) 
      access_syntax_error(reqInfo,n,
            "Too many security entries, increase MAX_SECURITY and recompile",
	    f,file);

    sec[x].opts=OPT_UNSET;
    sec[x].override = or;
    sec[x].bSatisfy = 0;

    if(is_matchexp(dir))
        strcpy(sec[x].d,dir);
    else
        strcpy_dir(sec[x].d,dir);

    sec[x].auth_type[0] = '\0';
    sec[x].auth_name[0] = '\0';
    sec[x].auth_pwfile[0] = '\0';
#ifdef DIGEST_AUTH
    sec[x].auth_digestfile[0] = '\0';
#endif /* DIGEST_AUTH */
    sec[x].auth_grpfile[0] = '\0';
    for(i=0;i<METHODS;i++) {
        sec[x].order[i] = DENY_THEN_ALLOW;
        sec[x].num_allow[i]=0;
        sec[x].num_deny[i]=0;
	sec[x].num_referer_allow[i]=0;
	sec[x].num_referer_deny[i]=0;
        sec[x].num_auth[i] = 0;
	sec[x].on_deny[i] = NULL;
    }

    while(!(cfg_getline(l,MAX_STRING_LEN,f))) {
        ++n;
        if((l[0] == '#') || (!l[0])) continue;
        cfg_getword(w,l);

        if(!strcasecmp(w,"AllowOverride")) {
            if(file)
                access_syntax_error(reqInfo,n,"override violation",f,file);
            sec[x].override = OR_NONE;
            while(l[0]) {
                cfg_getword(w,l);
                if(!strcasecmp(w,"Limit"))
                    sec[x].override |= OR_LIMIT;
                else if(!strcasecmp(w,"Options"))
                    sec[x].override |= OR_OPTIONS;
                else if(!strcasecmp(w,"FileInfo"))
                    sec[x].override |= OR_FILEINFO;
                else if(!strcasecmp(w,"AuthConfig"))
                    sec[x].override |= OR_AUTHCFG;
                else if(!strcasecmp(w,"Indexes"))
                    sec[x].override |= OR_INDEXES;
		else if(!strcasecmp(w,"Redirect"))
		    sec[x].override |= OR_REDIRECT;
                else if(!strcasecmp(w,"None"))
                    sec[x].override = OR_NONE;
                else if(!strcasecmp(w,"All")) 
                    sec[x].override = OR_ALL;
                else {
                    access_syntax_error(reqInfo,n,
"Unknown keyword in AllowOverride directive.",f,file);
                }
            }
        } 
        else if(!strcasecmp(w,"Options")) {
            if(!(or & OR_OPTIONS))
                access_syntax_error(reqInfo,n,"override violation",f,file);
            sec[x].opts = OPT_NONE;
            while(l[0]) {
                cfg_getword(w,l);
                if(!strcasecmp(w,"Indexes"))
                    sec[x].opts |= OPT_INDEXES;
                else if(!strcasecmp(w,"Includes"))
                    sec[x].opts |= OPT_INCLUDES;
                else if(!strcasecmp(w,"IncludesNOEXEC"))
                    sec[x].opts |= (OPT_INCLUDES | OPT_INCNOEXEC);
                else if(!strcasecmp(w,"FollowSymLinks"))
                    sec[x].opts |= OPT_SYM_LINKS;
                else if(!strcasecmp(w,"SymLinksIfOwnerMatch"))
                    sec[x].opts |= OPT_SYM_OWNER;
                else if(!strcasecmp(w,"execCGI"))
                    sec[x].opts |= OPT_EXECCGI;
                else if(!strcasecmp(w,"None")) 
                    sec[x].opts = OPT_NONE;
                else if(!strcasecmp(w,"All")) 
                    sec[x].opts = OPT_ALL;
                else {
                    access_syntax_error(reqInfo,n,
"Unknown keyword in Options directive.",f,file);
                }
            }
        }
        else if(!strcasecmp(w,"AuthName")) {
            if(!(or & OR_AUTHCFG))
                access_syntax_error(reqInfo,n,"override violation",f,file);
	    strcpy(sec[x].auth_name,l);
        }
        else if(!strcasecmp(w,"AuthType")) {
            if(!(or & OR_AUTHCFG))
                access_syntax_error(reqInfo,n,"override violation",f,file);
            cfg_getword(w,l);
	    strcpy(sec[x].auth_type, w);
        }
        else if(!strcasecmp(w,"AuthUserFile")) {
            if(!(or & OR_AUTHCFG))
                access_syntax_error(reqInfo,n,"override violation",f,file);
            cfg_getword(w,l);
	    strcpy(sec[x].auth_pwfile, w);
	    cfg_getword(w,l);
#ifdef DBM_SUPPORT
	    if (!strcasecmp (w, "dbm"))
		sec[x].auth_pwfile_type = AUTHFILETYPE_DBM;
	    else 
#endif /* DBM_SUPPORT */
#ifdef NIS_SUPPORT
          if (!strcasecmp (w, "nis"))
              sec[x].auth_pwfile_type = AUTHFILETYPE_NIS;
          else 
#endif /* NIS_SUPPORT */
		sec[x].auth_pwfile_type = AUTHFILETYPE_STANDARD;
        }
#ifdef DIGEST_AUTH
        else if(!strcasecmp(w,"AuthDigestFile")) {
            if(!(or & OR_AUTHCFG))
                access_syntax_error(reqInfo,n,"override violation",f,file);
            cfg_getword(w,l);
	    strcpy(sec[x].auth_digestfile, w);
	    cfg_getword(w,l);
#ifdef DBM_SUPPORT
	    if (!strcasecmp (w, "dbm"))
		sec[x].auth_digestfile_type = AUTHFILETYPE_DBM;
	    else 
#endif /* DBM_SUPPORT */
#ifdef NIS_SUPPORT
          if (!strcasecmp (w, "nis"))
              sec[x].auth_digestfile_type = AUTHFILETYPE_NIS;
          else 
#endif /* NIS_SUPPORT */
		sec[x].auth_digestfile_type = AUTHFILETYPE_STANDARD;
        }
#endif /* DIGEST_AUTH */
        else if(!strcasecmp(w,"AuthGroupFile")) {
            if(!(or & OR_AUTHCFG))
                access_syntax_error(reqInfo,n,"override violation",f,file);
            cfg_getword(w,l);
	    strcpy(sec[x].auth_grpfile, w);
	    cfg_getword(w,l);
#ifdef DBM_SUPPORT
	    if (!strcasecmp (w, "dbm"))
		sec[x].auth_grpfile_type = AUTHFILETYPE_DBM;
	    else 
#endif /* DBM_SUPPORT */
#ifdef NIS_SUPPORT
          if (!strcasecmp (w, "nis"))
              sec[x].auth_grpfile_type = AUTHFILETYPE_NIS;
          else 
#endif /* NIS_SUPPORT */
		sec[x].auth_grpfile_type = AUTHFILETYPE_STANDARD;
        }
        else if(!strcasecmp(w,"AddType")) {
            if(!(or & OR_FILEINFO))
                access_syntax_error(reqInfo,n,"override violation",f,file);
            cfg_getword(w,l);
            cfg_getword(w2,l);
            if((w[0] == '\0') || (w2[0] == '\0')) {
                access_syntax_error(reqInfo,n,
"AddType must be followed by a type, one space, then a file or extension.",
                                    f,file);
            }
            add_type(reqInfo,w2,w);
        }
        else if(!strcasecmp(w,"DefaultType")) {
            if(!(or & OR_FILEINFO))
                access_syntax_error(reqInfo,n,"override violation",f,file);
            cfg_getword(w,l);
            strcpy(local_default_type,w);
        }
        else if(!strcasecmp(w,"AddEncoding")) {
            if(!(or & OR_FILEINFO))
                access_syntax_error(reqInfo,n,"override violation",f,file);
            cfg_getword(w,l);
            cfg_getword(w2,l);
            if((w[0] == '\0') || (w2[0] == '\0')) {
                access_syntax_error(reqInfo,n,
"AddEncoding must be followed by a type, one space, then a file or extension.",
                                    f,file);
            }
            add_encoding(reqInfo,w2,w);
        }
        else if(!strcasecmp(w,"DefaultIcon")) {
            if(!(or & OR_INDEXES))
                access_syntax_error(reqInfo,n,"override violation",f,file);
            cfg_getword(w,l);
            strcpy(local_default_icon,w);
        }
        else if(!strcasecmp(w,"AddDescription")) {
            char desc[MAX_STRING_LEN];
            int fq;
            
            if(!(or & OR_INDEXES))
                access_syntax_error(reqInfo,n,"override violation",f,file);
            if((fq = ind(l,'\"')) == -1)
                access_syntax_error(reqInfo,n,"AddDescription must have quotes",
                                    f,file);
            else {
                getword(desc,&l[++fq],'\"');
                cfg_getword(w,&l[fq]);
                add_desc(reqInfo,local,BY_PATH,desc,w,sec[x].d);
            }
        }
        else if(!strcasecmp(w,"IndexIgnore")) {
            if(!(or & OR_INDEXES))
                access_syntax_error(reqInfo,n,"override violation",f,file);
            while(l[0]) {
                cfg_getword(w,l);
                add_ignore(reqInfo,local,w,sec[x].d);
            }
        }
        else if(!strcasecmp(w,"AddIcon")) {
            char w2[MAX_STRING_LEN];
            
            if(!(or & OR_INDEXES))
                access_syntax_error(reqInfo,n,"override violation",f,file);
            cfg_getword(w2,l);
            while(l[0]) {
                cfg_getword(w,l);
                add_icon(reqInfo,local,BY_PATH,w2,w,sec[x].d);
            }
        }
        else if(!strcasecmp(w,"AddIconByType")) {
            char w2[MAX_STRING_LEN];
            
            if(!(or & OR_INDEXES))
                access_syntax_error(reqInfo,n,"override violation",f,file);
            cfg_getword(w2,l);
            while(l[0]) {
                cfg_getword(w,l);
                add_icon(reqInfo,local,BY_TYPE,w2,w,sec[x].d);
            }
        }
        else if(!strcasecmp(w,"AddIconByEncoding")) {
            char w2[MAX_STRING_LEN];
            
            if(!(or & OR_INDEXES))
                access_syntax_error(reqInfo,n,"override violation",f,file);
            cfg_getword(w2,l);
            while(l[0]) {
                cfg_getword(w,l);
                add_icon(reqInfo,local,BY_ENCODING,w2,w,sec[x].d);
            }
        }
        else if(!strcasecmp(w,"ReadmeName")) {
            if(!(or & OR_INDEXES))
                access_syntax_error(reqInfo,n,"override violation",f,file);
            cfg_getword(w,l);
            add_readme(reqInfo,local,w,sec[x].d);
        }
        else if(!strcasecmp(w,"HeaderName")) {
            if(!(or & OR_INDEXES))
                access_syntax_error(reqInfo,n,"override violation",f,file);
            cfg_getword(w,l);
            add_header(reqInfo,local,w,sec[x].d);
        }
        else if(!strcasecmp(w,"IndexOptions")) {
            if(!(or & OR_INDEXES))
                access_syntax_error(reqInfo,n,"override violation",f,file);
            add_opts(reqInfo,local,l,sec[x].d);
        }
        else if(!strcasecmp(w,"Redirect") || !strcasecmp(w,"RedirectTemp")) {
            if(!(or & OR_REDIRECT))
                access_syntax_error(reqInfo,n,"override violation",f,file);
            cfg_getword(w,l);
            cfg_getword(w2,l);
            if((w[0] == '\0') || (w2[0] == '\0') || 
	       !(is_url(w2) || (w2[0] == '/')))
	    {
                access_syntax_error(reqInfo,n,
"Redirect must be followed by a document, one space, then a URL.",f,file);
            } 
            if(!file) {
	      if (w2[0] == '/') {
                char w3[MAX_STRING_LEN];
                construct_url(w3,gConfiguration,w2);
                strcpy(w2,w3);
              }
	      if (!set_host_conf_value(gConfiguration,PH_SRM_CONF,SRM_TRANSLATIONS)) {
		gConfiguration->translations = NULL;
	      }
	      add_redirect(gConfiguration,w,w2,A_REDIRECT_TEMP);
	    }
            else {
		char tmp[HUGE_STRING_LEN];
		int len;
	        if (w2[0] == '/') {
                   construct_url(tmp,reqInfo->hostInfo,w2);
	           strcpy(w2,tmp);
                }
		strcpy(tmp,reqInfo->url);
		if (reqInfo->path_info[0]) {
		  len = strlen(reqInfo->url);
		  if (reqInfo->url[len-1] == '/')
		    reqInfo->url[len-1] = '\0';
                  strcat(tmp,reqInfo->path_info);
		} 
		if (!strcmp(tmp,w) || !strcmp_match(tmp,w)) {
		  FClose(f);
		  die(reqInfo,SC_REDIRECT_TEMP,w2);
		}
	    }
        }
        else if(!strcasecmp(w,"RedirectPermanent")) {
            if(!(or & OR_FILEINFO))
                access_syntax_error(reqInfo,n,"override violation",f,file);
            cfg_getword(w,l);
            cfg_getword(w2,l);
            if((w[0] == '\0') || (w2[0] == '\0') || 
	       !(is_url(w2) || (w2[0] == '/'))) 
	    {
                access_syntax_error(reqInfo,n,
"Redirect must be followed by a document, one space, then a URL.",f,file);
            }
            if(!file) {
	      if (w2[0] == '/') {
                char w3[MAX_STRING_LEN];
                construct_url(w3,gConfiguration,w2);
                strcpy(w2,w3);
              }
	      if (!set_host_conf_value(gConfiguration,PH_SRM_CONF,SRM_TRANSLATIONS)) {
		gConfiguration->translations = NULL;
	      }
	      add_redirect(gConfiguration,w,w2,A_REDIRECT_PERM);
	    }
            else {
                char tmp[HUGE_STRING_LEN];
                int len;
                if (w2[0] == '/') {
                   construct_url(tmp,reqInfo->hostInfo,w2);
                   strcpy(w2,tmp);
                }
                strcpy(tmp,reqInfo->url);
                if (reqInfo->path_info[0]) {
                  len = strlen(reqInfo->url);
                  if (reqInfo->url[len-1] == '/')
                    reqInfo->url[len-1] = '\0';
                  strcat(tmp,reqInfo->path_info);
                }
                if (!strcmp(tmp,w) || !strcmp_match(tmp,w)) {
                  FClose(f);
                  die(reqInfo,SC_REDIRECT_TEMP,w2);
                }
            }
	}
        else if(!strcasecmp(w,"<Limit")) {
            int m[METHODS];

            if(!(or & OR_LIMIT))
                access_syntax_error(reqInfo,n,"override violation",f,file);
            for(i=0;i<METHODS;i++) m[i] = 0;
            getword(w2,l,'>');
            while(w2[0]) {
                cfg_getword(w,w2);
                if(!strcasecmp(w,"GET")) m[M_GET]=1;
                else if(!strcasecmp(w,"PUT")) m[M_PUT]=1;
                else if(!strcasecmp(w,"POST")) m[M_POST]=1;
                else if(!strcasecmp(w,"DELETE")) m[M_DELETE]=1;
            }
            while(1) {
                if(cfg_getline(l,MAX_STRING_LEN,f))
                    access_syntax_error(reqInfo,n,"Limit missing /Limit",f,file);
                n++;
                if((l[0] == '#') || (!l[0])) continue;

                if(!strcasecmp(l,"</Limit>"))
                    break;
                cfg_getword(w,l);
                if(!strcasecmp(w,"order")) {
                    if(!strcasecmp(l,"allow,deny")) {
                        for(i=0;i<METHODS;i++)
                            if(m[i])
                                sec[x].order[i] = ALLOW_THEN_DENY;
                    }
                    else if(!strcasecmp(l,"deny,allow")) {
                        for(i=0;i<METHODS;i++)
                            if(m[i]) 
                                sec[x].order[i] = DENY_THEN_ALLOW;
                    }
                    else if(!strcasecmp(l,"mutual-failure")) {
                        for(i=0;i<METHODS;i++)
                            if(m[i]) 
                                sec[x].order[i] = MUTUAL_FAILURE;
                    }
                    else
                        access_syntax_error(reqInfo,n,"Unknown order.",f,file);
                } 
                else if((!strcasecmp(w,"allow"))) {
                    cfg_getword(w,l);
                    if(strcmp(w,"from"))
                        access_syntax_error(reqInfo,n,
                                            "allow must be followed by from.",
                                            f,file);
                    while(1) {
                        cfg_getword(w,l);
                        if(!w[0]) break;
                        for(i=0;i<METHODS;i++)
                            if(m[i]) {
                                int q=sec[x].num_allow[i]++;
				if (q >= MAX_SECURITY)
                                  access_syntax_error(reqInfo,n,
	  "Too many allow entries, increase MAX_SECURITY and recompile",
			                              f,file);
                                if(!(sec[x].allow[i][q] = strdup(w)))
                                    die(reqInfo,SC_NO_MEMORY,"parse_access_dir");
                            }
                    }
                }
		else if(!strcasecmp(w,"referer")) {
		    int ref_type; 

		    cfg_getword(w,l);
		    if(!strcmp(w,"allow")) {
		      ref_type = FA_ALLOW; 
		    } else if (!strcmp(w,"deny")) {
		      ref_type = FA_DENY;
		    } else access_syntax_error(reqInfo,n,
					       "unknown referer type.",
					       f,file);
                    cfg_getword(w,l);
  		    if(strcmp(w,"from"))
		      access_syntax_error(reqInfo,n,
				          "allow/deny must be followed by from.",
					  f,file);
		    while(1) {
		      cfg_getword(w,l);
		      if (!w[0]) break;
		      for(i=0;i<METHODS;i++)
			if(m[i]) {
			  int q;
			  if (ref_type == FA_ALLOW) {
			    q=sec[x].num_referer_allow[i]++;
                            if (q >= MAX_SECURITY)
			      access_syntax_error(reqInfo,n,
    "Too many referer allow entries, increase MAX_SECURITY and recompile",
		                                  f,file);
			    if(!(sec[x].referer_allow[i][q] = strdup(w)))
			      die(reqInfo,SC_NO_MEMORY,"parse_access_dir");
			  } else if (ref_type == FA_DENY) {
			    q=sec[x].num_referer_deny[i]++;
                            if (q >= MAX_SECURITY)
                              access_syntax_error(reqInfo,n,
    "Too many referer deny entries, increase MAX_SECURITY and recompile",
                                                  f,file);
			    if(!(sec[x].referer_deny[i][q] = strdup(w)))
			      die(reqInfo,SC_NO_MEMORY,"parse_access_dir");
                          }
                        }
                    }
                }
                else if(!strcasecmp(w,"require")) {
                    for(i=0;i<METHODS;i++)
                         if(m[i]) {
                            int q=sec[x].num_auth[i]++;
                            if (q >= MAX_SECURITY)
                              access_syntax_error(reqInfo,n,
 	    "Too many require entries, increase MAX_SECURITY and recompile",
                                                  f,file);
                            if(!(sec[x].auth[i][q] = strdup(l)))
                                die(reqInfo,SC_NO_MEMORY,"parse_access_dir");
                        }
                }
                else if((!strcasecmp(w,"deny"))) {
                    cfg_getword(w,l);
                    if(strcmp(w,"from"))
                        access_syntax_error(reqInfo,n,
                                            "deny must be followed by from.",
                                            f,file);
                    while(1) {
                        cfg_getword(w,l);
                        if(!w[0]) break;
                        for(i=0;i<METHODS;i++)
                            if(m[i]) {
                                int q=sec[x].num_deny[i]++;
                                if (q >= MAX_SECURITY)
                                  access_syntax_error(reqInfo,n,
    		"Too many deny entries, increase MAX_SECURITY and recompile",
                                                      f,file);
                                if(!(sec[x].deny[i][q] = strdup(w)))
                                    die(reqInfo,SC_NO_MEMORY,"parse_access_dir");
                            }
                    }
                }
		else if((!strcasecmp(w, "satisfy"))){
		    sec[x].bSatisfy = SATISFY_ALL;       /*default 0 for all*/
		    cfg_getword(w,l);
		    if(!w[0]) break;   /*if not specified, assume satisfy all*/
		    if ((!strcasecmp(w, "any")))
			sec[x].bSatisfy = SATISFY_ANY;
		    else if ((strcasecmp(w, "all")))/* unknow word in satisfy*/
			access_syntax_error(reqInfo,n, 
					    "Satisfy either any or all.", 
					    f,file);
		}
		else if(!strcasecmp(w,"OnDeny")) {
                  for(i=0;i<METHODS;i++)
                     if(m[i]) {
                       if(!(sec[x].on_deny[i] = strdup(l)))
                         die(reqInfo,SC_NO_MEMORY,"parse_access_dir");
                     }
                }
                else
                    access_syntax_error(reqInfo,n,
					"Unknown keyword in Limit region.",
                                        f,file);
            }
        }
        else if(!strcasecmp(w,"</Directory>"))
            break;
        else {
            char errstr[MAX_STRING_LEN];
            sprintf(errstr,"Unknown method %s",w);
            access_syntax_error(reqInfo,n,errstr,f,file);
            return -1;
        }
    }
    ++num_sec;
    return n;
}


void parse_htaccess(per_request *reqInfo, char *path, char override) 
{
    struct stat buf;
    FILE *f;
    char t[MAX_STRING_LEN];
    char d[MAX_STRING_LEN];

    strcpy(d,path);
    make_full_path(d,reqInfo->hostInfo->access_name,t);

    if((stat(t, &buf) != -1) && (f=FOpen(t,"r"))) {
        parse_access_dir(reqInfo,f,-1,override,d,t,FI_LOCAL);
        FClose(f);
    }
}


void process_access_config(FILE *errors) 
{
    FILE *f;
    char l[MAX_STRING_LEN];
    char w[MAX_STRING_LEN];
    int n;
    per_request reqInfo;

    reqInfo.out = errors;
    num_sec = 0;
    n=0;
    if(!(f=fopen(access_confname,"r"))) {
        fprintf(errors,"HTTPd: could not open access configuration file %s.\n",
                access_confname);
        perror("fopen");
        exit(1);
    }
    while(!(cfg_getline(l,MAX_STRING_LEN,f))) {
        ++n;
        if((l[0] == '#') || (!l[0])) continue;
	cfg_getword(w,l);
	if(strcasecmp(w,"<Directory")) {
	    fprintf(errors,
		    "Syntax error on line %d of access config. file.\n",n);
	    fprintf(errors,"Unknown directive %s.\n",w);
	    exit(1);
	}
	getword(w,l,'>');
	n=parse_access_dir(&reqInfo,f,n,OR_ALL,w,NULL,FI_GLOBAL);
    }
    fclose(f);
    num_sec_config = num_sec;
}

void read_config(FILE *errors) 
{
  FILE *cfg;
  per_host *host;

  host = create_host_conf(NULL,FALSE);
  set_defaults(host,errors);

  if(!(cfg = fopen(server_confname,"r"))) {
    fprintf(errors,
	    "HTTPd: could not open server config. file %s\n",
	    server_confname);
    perror("fopen");
    exit(1);
  }
  init_indexing(FI_GLOBAL);
  process_server_config(host,cfg,errors,FALSE);
  fclose(cfg);
  init_mime();
  /*  process_resource_config(errors); */
  process_access_config(errors);
  open_all_logs();
}
