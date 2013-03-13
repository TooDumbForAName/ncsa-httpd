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
 * host_config.h,v 1.9 1995/11/28 09:01:47 blong Exp
 *
 ************************************************************************
 *
 * host_config.h
 *
 *
 */

#ifndef _HOST_CONFIG_H_
#define _HOST_CONFIG_H_ 1

/* HTTPD_CONF OPTS - Needs at least 16 bit ints 
   if set, means that variable is set to something other than default */

#define HC_LOG_TYPE	   1
#define HC_ERROR_FNAME     2
#define HC_XFER_FNAME      4
#define HC_AGENT_FNAME     8
#define HC_REFERER_FNAME   16
#define HC_REFERER_IGNORE  32
#define HC_SERVER_ADMIN    128
#define HC_SERVER_HOSTNAME 256
#define HC_SRM_CONFNAME    512
#define HC_ANNOT_SERVER    4096

/* SRM_CONF OPTS */
#define SRM_USER_DIR       1
#define SRM_INDEX_NAMES    2
#define SRM_ACCESS_NAME    4
#define SRM_DOCUMENT_ROOT  8
#define SRM_DEFAULT_TYPE   16
#define SRM_DEFAULT_ICON   32
#define SRM_TRANSLATIONS   64
#define SRM_DOCERRORS	   128

/* PEM_CONF OPTS */
#define PEMC_PEM_ENCRYPT   1
#define PEMC_PEM_DECRYPT   2
#define PEMC_PEM_ENTITY    4
#define PEMC_PGP_ENCRYPT   8
#define PEMC_PGP_DECRYPT   16
#define PEMC_PGP_ENTITY    32

/* globals defined in this module */
extern per_host *gConfiguration;

/* function defined in this module */
per_host* create_host_conf(per_host *hostInfo, int virtual);
void free_host_conf(void);
int set_host_conf_value(per_host *hostInfo, int part, int option);
void set_host_conf(per_host *hostInfo, int part, int option, char *value);
void which_host_conf(per_request *reqInfo);
void open_all_logs(void);
void close_all_logs(void);

#endif /* _HOST_CONFIG_H_ */

