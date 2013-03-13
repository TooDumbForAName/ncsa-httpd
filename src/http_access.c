/*
 * http_access: Security options etc.
 * 
 * Rob McCool
 * 
 */


#include "httpd.h"

int in_domain(char *domain, char *what) {
    int dl=strlen(domain);
    int wl=strlen(what);

    if((wl-dl) >= 0)
        return(!strcmp(domain,&what[wl-dl]));
    else
        return 0;
}

int in_ip(char *domain, char *what) {
    return(!strncmp(domain,what,strlen(domain)));
}

int find_allow(int x, int m) {
    register int y;

    if(sec[x].num_allow[m] < 0)
        return 1;

    for(y=0;y<sec[x].num_allow[m];y++) {
        if(!strcmp("all",sec[x].allow[m][y])) 
            return 1;
        if(remote_host && isalpha(remote_host[0]))
            if(in_domain(sec[x].allow[m][y],remote_host))
                return 1;
        if(in_ip(sec[x].allow[m][y],remote_ip))
            return 1;
    }
    return 0;
}

int find_deny(int x, int m) {
    register int y;

    if(sec[x].num_deny[m] < 0)
        return 1;

    for(y=0;y<sec[x].num_deny[m];y++) {
        if(!strcmp("all",sec[x].deny[m][y])) 
            return 1;
        if(remote_host && isalpha(remote_host[0]))
            if(in_domain(sec[x].deny[m][y],remote_host))
                return 1;
        if(in_ip(sec[x].deny[m][y],remote_ip))
            return 1;
    }
    return 0;
}

void check_dir_access(int x, int m, int *w, int *n) {
    if(sec[x].auth_type)
        auth_type = sec[x].auth_type;
    if(sec[x].auth_name)
        auth_name = sec[x].auth_name;
    if(sec[x].auth_pwfile)
        auth_pwfile = sec[x].auth_pwfile;
    if(sec[x].auth_grpfile)
        auth_grpfile = sec[x].auth_grpfile;

    if(sec[x].order[m] == ALLOW_THEN_DENY) {
        if(find_allow(x,m))
            *w=1;
        if(find_deny(x,m))
            *w=0;
    } else if(sec[x].order[m] == DENY_THEN_ALLOW) {
        if(find_deny(x,m))
            *w=0;
        if(find_allow(x,m))
            *w=1;
    }
    else
        *w = find_allow(x,m) && (!find_deny(x,m));

    if(sec[x].num_auth[m])
        *n=x;
}

void evaluate_access(char *p, struct stat *finfo, int m, int *allow, 
                     char *allow_options,FILE *out)
{
    int will_allow, need_auth, num_dirs;
    char opts[MAX_STRING_LEN], override[MAX_STRING_LEN];
    char path[MAX_STRING_LEN], d[MAX_STRING_LEN];
    char errstr[MAX_STRING_LEN];
    register int x,y,z,n;

    if(S_ISDIR(finfo->st_mode)) strcpy_dir(path,p);
    else strcpy(path,p);

    no2slash(path);

    num_dirs = count_dirs(path);
    will_allow=1;need_auth=-1;
    auth_type=NULL;auth_name=NULL;auth_pwfile=NULL;auth_grpfile=NULL;
    user[0] = '\0';
    for(x=0;x<num_dirs;x++) {
        opts[x] = OPT_ALL;
        override[x] = OR_ALL;
    }

    for(x=0;x<num_sec;x++) {
        if((is_matchexp(path) ? strcmp_match(path,sec[x].d)
                              : !strncmp(path,sec[x].d,strlen(sec[x].d))))
            {
                for(y=count_dirs(sec[x].d) - 1;y<num_dirs;y++) {
                    if(!(sec[x].opts & OPT_UNSET))
                        opts[y] = sec[x].opts;
                    override[y] = sec[x].override;
                }
                check_dir_access(x,m,&will_allow,&need_auth);
            }
    }
    n=num_dirs-1;
    if((override[n]) || (!(opts[n] & OPT_SYM_LINKS)) || 
       (opts[n] & OPT_SYM_OWNER))
        {
            for(x=0;x<num_dirs;x++) {
                y = num_sec;
                make_dirstr(path,x+1,d);
                if((!(opts[x] & OPT_SYM_LINKS)) || (opts[x] & OPT_SYM_OWNER)) {
                    struct stat lfi,fi;

                    lstat(d,&lfi);
                    if(!(S_ISDIR(lfi.st_mode))) {
                        if(opts[x] & OPT_SYM_OWNER) {
                            char realpath[512];
                            int bsz;

                            if((bsz = readlink(d,realpath,256)) == -1)
                                goto bong;
                            realpath[bsz] = '\0';
                            if(realpath[0] != '/') {
                                char t[256];
                                strcpy(t,"../");
                                strcpy(&t[3],realpath);
                                make_full_path(d,t,realpath);
                                getparents(realpath);
                            }
                            lstat(realpath,&fi);
                            if(fi.st_uid != lfi.st_uid)
                                goto bong;
                        }
                        else {
                          bong:
                            sprintf(errstr,"httpd: will not follow link %s",d);
                            log_error(errstr);
                            *allow=0;
                            *allow_options = OPT_NONE;
                            return;
                        }
                    }
                }
                if(override[x]) {
                    parse_htaccess(d,override[x],out);
                    if(num_sec != y) {
                        for(z=count_dirs(sec[y].d) - 1;z<num_dirs;z++) {
                            if(!(sec[y].opts & OPT_UNSET))
                                opts[z] = sec[y].opts;
                            override[z] = sec[y].override;
                        }
                        check_dir_access(y,m,&will_allow,&need_auth);
                    }
                }
            }
        }
    if((!(S_ISDIR(finfo->st_mode))) && 
       ((!(opts[n] & OPT_SYM_LINKS)) || (opts[x] & OPT_SYM_OWNER))) {
        struct stat fi,lfi;
        lstat(path,&fi);
        if(!(S_ISREG(fi.st_mode))) {
            if(opts[n] & OPT_SYM_OWNER) {
                char realpath[512];
                int bsz;
                
                if((bsz = readlink(path,realpath,256)) == -1)
                    goto gong;
                realpath[bsz] = '\0';
                if(realpath[0] != '/') {
                    char t[256];
                    strcpy(t,"../");
                    strcpy(&t[3],realpath);
                    make_full_path(path,t,realpath);
                    getparents(realpath);
                }
                lstat(realpath,&lfi);
                if(fi.st_uid != lfi.st_uid)
                    goto gong;
            }
            else {
              gong:
                sprintf(errstr,"httpd: will not follow link %s",path);
                log_error(errstr);
                *allow=0;
                *allow_options = OPT_NONE;
                return;
            }
        }
    }
    *allow = will_allow;
    if(will_allow) {
        *allow_options = opts[num_dirs-1];
        if(need_auth >= 0)
            check_auth(&sec[need_auth],m,out);
    }
    else *allow_options = 0;
}

void kill_security() {
    register int x,y,m;

    for(x=0;x<num_sec;x++) {
        free(sec[x].d);
        for(m=0;m<METHODS;m++) {
            for(y=0;y<sec[x].num_allow[m];y++)
                free(sec[x].allow[m][y]);
            for(y=0;y<sec[x].num_deny[m];y++)
                free(sec[x].deny[m][y]);
            for(y=0;y<sec[x].num_auth[m];y++)
                free(sec[x].auth[m][y]);
        }
        if(sec[x].auth_type)
            free(sec[x].auth_type);
        if(sec[x].auth_name)
            free(sec[x].auth_name);
        if(sec[x].auth_pwfile)
            free(sec[x].auth_pwfile);
        if(sec[x].auth_grpfile)
            free(sec[x].auth_grpfile);
    }
}


