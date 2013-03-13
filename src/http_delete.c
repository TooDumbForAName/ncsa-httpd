/*
 * http_delete.c: Handles DELETE
 * 
 * All code contained herein is covered by the Copyright as distributed
 * in the README file in the main directory of the distribution of 
 * NCSA HTTPD.
 *
 * Based on NCSA HTTPd 1.3 by Rob McCool
 * 
 */

#include "httpd.h"


void handle_delete(char *name, char *args, int in, FILE *out) {
    struct stat finfo;
    char ct_bak[MAX_STRING_LEN];

    if(stat(name,&finfo) == -1) {
        if(find_script("DELETE",name,args,in,out))
            return;
        if(errno==ENOENT) {
            log_reason("file does not exist",name);
            unmunge_name(name);
            die(NOT_FOUND,name,out);
        }
        else {
            log_reason("file permissions deny server access",name);
            unmunge_name(name);
            die(FORBIDDEN,name,out);
        }
    }
    probe_content_type(name);
    if(!strcmp(content_type,CGI_MAGIC_TYPE)) {
        strcpy(content_type,ct_bak);
        send_cgi("DELETE",name,"",args,&finfo,in,out);
        return;
    }
    /* Not a script, do group ann thang */
    die(NOT_IMPLEMENTED,"DELETE to non-script",out);
}


void delete_node(char *name, char *args, int in, FILE *out) {
    int s;

    s=translate_name(name,out);

    switch(s) {
      case STD_DOCUMENT:
        handle_delete(name,args,in,out);
        return;
      case REDIRECT_URL:
        die(REDIRECT,name,out);
      case SCRIPT_CGI:
        exec_cgi_script("DELETE",name,args,in,out);
      default:
        die(NOT_IMPLEMENTED,"NCSA script exeuction of delete",out);
    }
}
