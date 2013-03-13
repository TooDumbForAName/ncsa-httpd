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
 *host_config.c,v 1.15 1995/11/06 20:57:59 blong Exp
 *
 ************************************************************************
 *
 */

#include "config.h"
#include "portability.h"

#include "constants.h"
#ifndef NO_STDLIB_H 
# include <stdlib.h>
#endif /* NO_STDLIB_H */
#include <string.h>
#include "host_config.h"
#include "http_log.h"
#include "http_alias.h"
#include "http_mime.h"
#include "util.h"

per_host* gConfiguration;

per_host* create_host_conf(per_host *hostInfo, int virtual) {
  per_host *newInfo, *tmpInfo;

  newInfo = (per_host *) malloc(sizeof(per_host));

  newInfo->httpd_conf = 0;
  newInfo->srm_conf = 0;
  
  newInfo->files_open = 0;
  newInfo->next = NULL;

  if (virtual) newInfo->virtualhost = TRUE;
    else newInfo->virtualhost = FALSE;
  newInfo->called_hostname = NULL;

  if (hostInfo != NULL) {
    if (hostInfo->files_open != TRUE) {
      open_logs(hostInfo);
      hostInfo->files_open = TRUE;
    }
    newInfo->error_fname = hostInfo->error_fname;
    newInfo->xfer_fname = hostInfo->xfer_fname;
    newInfo->agent_fname = hostInfo->agent_fname;
    newInfo->referer_fname = hostInfo->referer_fname;
    newInfo->referer_ignore = hostInfo->referer_ignore;
    newInfo->server_admin = hostInfo->server_admin;
    newInfo->server_hostname = hostInfo->server_hostname;
    newInfo->srm_confname = hostInfo->srm_confname;
    newInfo->annotation_server = hostInfo->annotation_server;
    newInfo->dns_mode = hostInfo->dns_mode;
    
    newInfo->log_opts = hostInfo->log_opts;
    newInfo->error_log = hostInfo->error_log;
    newInfo->agent_log = hostInfo->agent_log;
    newInfo->referer_log = hostInfo->referer_log;
    newInfo->xfer_log = hostInfo->xfer_log;
    
    newInfo->user_dir = hostInfo->user_dir;
    newInfo->index_names = hostInfo->index_names;
    newInfo->access_name = hostInfo->access_name;
    newInfo->document_root = hostInfo->document_root;
    newInfo->doc_root_len = hostInfo->doc_root_len;
    newInfo->default_type = hostInfo->default_type;
    newInfo->default_icon = hostInfo->default_icon;

    newInfo->num_doc_errors = hostInfo->num_doc_errors;
    newInfo->doc_errors = hostInfo->doc_errors;
    newInfo->translations = hostInfo->translations;
    
    
    /* thanks to Kevin Ruddy (smiles@powerdog.com) for re-teaching me
       how to make a linked list */

    tmpInfo = hostInfo;
    while (tmpInfo->next != NULL) tmpInfo = tmpInfo->next;
    tmpInfo->next = newInfo;
  } else {
    gConfiguration = newInfo;
    newInfo->translations = NULL;
    newInfo->num_doc_errors = 0;
  }

  return newInfo;
}

void free_host_conf() {
  per_host *host = gConfiguration;
  per_host *tmp;

  while (host != NULL) {
    close_logs(host);
    if (host->httpd_conf & HC_ERROR_FNAME) free(host->error_fname);
    if (host->httpd_conf & HC_XFER_FNAME) free(host->xfer_fname);
    if (host->httpd_conf & HC_AGENT_FNAME) free(host->agent_fname);
    if (host->httpd_conf & HC_REFERER_FNAME) free(host->referer_fname);
    if (host->httpd_conf & HC_REFERER_IGNORE) free(host->referer_ignore);
    if (host->httpd_conf & HC_SERVER_ADMIN) free(host->server_admin);
    if (host->httpd_conf & HC_SERVER_HOSTNAME) free(host->server_hostname);
    if (host->httpd_conf & HC_SRM_CONFNAME) free(host->srm_confname);
    if (host->httpd_conf & HC_ANNOT_SERVER) free(host->annotation_server);
    if (host->srm_conf & SRM_USER_DIR) free(host->user_dir);
    if (host->srm_conf & SRM_INDEX_NAMES) free(host->index_names);
    if (host->srm_conf & SRM_ACCESS_NAME) free(host->access_name);
    if (host->srm_conf & SRM_DOCUMENT_ROOT) free(host->document_root);
    if (host->srm_conf & SRM_DEFAULT_TYPE) free(host->default_type);
    if (host->srm_conf & SRM_DEFAULT_ICON) free(host->default_icon);
    if (host->srm_conf & SRM_TRANSLATIONS) free_aliases(host->translations);
    if (host->srm_conf & SRM_DOCERRORS) free_doc_errors(host);
    
    tmp = host->next;
    free(host);
    host = tmp;
  }
}

/* set_host_conf_value will add an option to the list, and return the former
   value of the option.  Used for non-string values stored in per_host
   data structure */

int set_host_conf_value(per_host *hostInfo, int part, int option) 
{
    int tmp = 0;

    switch(part) {
      case PH_HTTPD_CONF:
	tmp = hostInfo->httpd_conf & option;
	hostInfo->httpd_conf = hostInfo->httpd_conf | option;
	break;
      case PH_SRM_CONF:
	tmp = hostInfo->srm_conf & option;
	hostInfo->srm_conf = hostInfo->srm_conf | option;
	break;
    }
    return tmp;
}

void set_host_conf(per_host *hostInfo, int part, int option, char *value) {
  char *tmp;

  tmp = strdup(value);
  
  switch(part) {
  case PH_HTTPD_CONF:
    switch (option) {
    case HC_ERROR_FNAME:
      if (hostInfo->httpd_conf & option) 
	free(hostInfo->error_fname);
      hostInfo->error_fname = tmp;
      break;
    case HC_XFER_FNAME:
      if (hostInfo->httpd_conf & option) 
	free(hostInfo->xfer_fname);
      hostInfo->xfer_fname = tmp;
      break;
    case HC_AGENT_FNAME:
      if (hostInfo->httpd_conf & option) 
	free(hostInfo->agent_fname);
      hostInfo->agent_fname = tmp;
      break;
    case HC_REFERER_FNAME:
      if (hostInfo->httpd_conf & option) 
	free(hostInfo->referer_fname);
      hostInfo->referer_fname = tmp;
      break;
    case HC_REFERER_IGNORE:
      if (hostInfo->httpd_conf & option) 
	free(hostInfo->referer_ignore);
      hostInfo->referer_ignore = tmp;
      break;
    case HC_SERVER_ADMIN:
      if (hostInfo->httpd_conf & option) 
	free(hostInfo->server_admin);
      hostInfo->server_admin = tmp;
      break;
    case HC_SERVER_HOSTNAME:
      if (hostInfo->httpd_conf & option) 
	free(hostInfo->server_hostname);
      hostInfo->server_hostname = tmp;
      break;
    case HC_SRM_CONFNAME:
      if (hostInfo->httpd_conf & option) 
	free(hostInfo->srm_confname);
      hostInfo->srm_confname = tmp;
      break;
    case HC_ANNOT_SERVER:
      if (hostInfo->httpd_conf & option) 
	free(hostInfo->annotation_server);
      hostInfo->annotation_server = tmp;
      break;
    }
    hostInfo->httpd_conf = hostInfo->httpd_conf | option;
    break;
  case PH_SRM_CONF:
    switch (option) {
    case SRM_USER_DIR:
      if (hostInfo->srm_conf & option)
	free(hostInfo->user_dir);
      hostInfo->user_dir = tmp;
      break;
    case SRM_INDEX_NAMES:
      if (hostInfo->srm_conf & option)
	free(hostInfo->index_names);
      hostInfo->index_names = tmp;
      break;
    case SRM_ACCESS_NAME:
      if (hostInfo->srm_conf & option)
	free(hostInfo->access_name);
      hostInfo->access_name = tmp;
      break;
    case SRM_DOCUMENT_ROOT:
      if (hostInfo->srm_conf & option)
	free(hostInfo->document_root);
      hostInfo->document_root = tmp;
      hostInfo->doc_root_len = strlen(tmp);
      break;
    case SRM_DEFAULT_TYPE:
      if (hostInfo->srm_conf & option)
	free(hostInfo->default_type);
      hostInfo->default_type = tmp;
      break;
    case SRM_DEFAULT_ICON:
      if (hostInfo->srm_conf & option)
	free(hostInfo->default_icon);
      hostInfo->default_icon = tmp;
      break;
    }
    hostInfo->srm_conf = hostInfo->srm_conf | option;
    break;
  }
}

void open_all_logs() {
  per_host *host = gConfiguration;

  while (host != NULL) {
    if (!host->files_open) {
      open_logs(host);
      host->files_open = TRUE;
    }
    host = host->next;
  }
}

/* Close without cleanup, should only be used by the error handlers,
   (ie: bus_error and seg_fault) */
void close_all_logs() {
  per_host *host = gConfiguration;

  while (host != NULL) {
    close_logs(host);
    host = host->next;
  }
}

/* Calls get_local_addr from util.c to determine the called address
   of the connection, and then hunts for that down the host configuration
   linked list.  If none match, uses the first one (which should be INADDR_ANY,
   anyways) */
void which_host_conf(per_request *reqInfo) {
  per_host *host = gConfiguration;
  int Found = FALSE;

  get_local_addr(reqInfo);
  while (host && !Found) {
    if (host->address_info.s_addr == reqInfo->address_info.s_addr) {
      if (!host->virtualhost && host->called_hostname) {
	if (!strcasecmp(called_hostname,host->called_hostname)) Found = TRUE;
      } else Found = TRUE;	
    }	  
    if (!Found) host = host->next;
  }

  if (host == NULL) 
    reqInfo->hostInfo = gConfiguration;
  else
    reqInfo->hostInfo = host;
}
