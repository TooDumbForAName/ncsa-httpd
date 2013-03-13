/*
 * http_put.c: Handles PUT and POST
 * 
 * Rob McCool
 * 
 */

#include "httpd.h"


void get_node(char *name, char *args, int in, FILE *out) {
    struct stat finfo;
    int s;

    s=translate_name(name,out);

    switch(s) {
      case STD_DOCUMENT:
        die(NOT_IMPLEMENTED,
            "POST access to area not configured as script area",out);
      case REDIRECT_URL:
        die(REDIRECT,name,out);
      case SCRIPT_NCSA:
        exec_post_NCSA(name,args,in,out);
        return;
      case SCRIPT_CGI:
        exec_cgi_script("POST",name,args,in,out);
    }
}
