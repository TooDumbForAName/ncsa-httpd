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
 * http_dir.h,v 1.6 1995/11/28 09:02:01 blong Exp
 *
 ************************************************************************
 *
 *  http_dir.h contains the following information including:
 *        struct ent
 *        struct item
 *
 */


#ifndef _HTTP_DIR_H_
#define _HTTP_DIR_H_

/* constants used in this module */

/* Fancy Indexing Locality */
#define FI_LOCAL	0
#define FI_GLOBAL	1

/* Fancy Indexing Options */
#define ICONS_ARE_LINKS 	2
#define SCAN_HTML_TITLES 	4
#define SUPPRESS_LAST_MOD 	8
#define SUPPRESS_SIZE 		16
#define SUPPRESS_DESC 		32

struct ent {
    char *name;
    char *icon;
    char *alt;
    char *desc;
    size_t size;
    time_t lm;
    struct ent *next;
};

struct item {
    int type;
    char *apply_to;
    char *apply_path;
    char *data;
    struct item *next;
};

/* http_dir function prototypes */
void index_directory(per_request *reqInfo);
void add_icon(per_request *reqInfo, int local, int type, char *icon, char *to, char *path);
void add_alt(per_request *reqInfo, int local, int type, char *alt, char *to, char *path);
void add_desc(per_request *reqInfo, int local, int type, char *desc, char *to, char *path);
void add_ignore(per_request *reqInfo, int local, char *ext, char *path);
void add_header(per_request *reqInfo, int local, char *name, char *path);
void add_readme(per_request *reqInfo, int local, char *name, char *path);
void add_opts(per_request *reqInfo, int local, char *optstr, char *path);
void add_opts_int(per_request *reqInfo, int local, int opts, char *path);
void send_size(per_request *reqInfo, size_t size);

void init_indexing(int local);
void kill_indexing(int local);

#endif /* _HTTP_DIR_H_ */
