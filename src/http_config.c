/*
 * http_config.c: auxillary functions for reading httpd's config file
 * and converting filenames into a namespace
 *
 * Rob McCool 
 * 
 */

#include "httpd.h"


/* Server config globals */
int standalone;
int port;
uid_t user_id;
gid_t group_id;
char server_root[MAX_STRING_LEN];
char error_fname[MAX_STRING_LEN];
char xfer_fname[MAX_STRING_LEN];
char pid_fname[MAX_STRING_LEN];
char server_admin[MAX_STRING_LEN];
char *server_hostname;
char srm_confname[MAX_STRING_LEN];
char server_confname[MAX_STRING_LEN];
char access_confname[MAX_STRING_LEN];
char types_confname[MAX_STRING_LEN];
int timeout;
int do_rfc931;
#ifdef PEM_AUTH
char auth_pem_decrypt[MAX_STRING_LEN];
char auth_pem_encrypt[MAX_STRING_LEN];
char auth_pem_entity[MAX_STRING_LEN];
char auth_pgp_decrypt[MAX_STRING_LEN];
char auth_pgp_encrypt[MAX_STRING_LEN];
char auth_pgp_entity[MAX_STRING_LEN];
#endif

void process_server_config(FILE *errors) {
    FILE *cfg;
    char l[MAX_STRING_LEN],w[MAX_STRING_LEN];
    int n=0;

    standalone = 1;
    port = DEFAULT_PORT;
    user_id = uname2id(DEFAULT_USER);
    group_id = gname2id(DEFAULT_GROUP);
    /* ServerRoot set in httpd.c */
    make_full_path(server_root,DEFAULT_ERRORLOG,error_fname);
    make_full_path(server_root,DEFAULT_XFERLOG,xfer_fname);
    make_full_path(server_root,DEFAULT_PIDLOG,pid_fname);
    strcpy(server_admin,DEFAULT_ADMIN);
    server_hostname = NULL;
    make_full_path(server_root,RESOURCE_CONFIG_FILE,srm_confname);
    /* server_confname set in httpd.c */
    make_full_path(server_root,ACCESS_CONFIG_FILE,access_confname);
    make_full_path(server_root,TYPES_CONFIG_FILE,types_confname);

    timeout = DEFAULT_TIMEOUT;
    do_rfc931 = DEFAULT_RFC931;
#ifdef PEM_AUTH
    auth_pem_encrypt[0] = '\0';
    auth_pem_decrypt[0] = '\0';
    auth_pem_entity[0] = '\0';
    auth_pgp_encrypt[0] = '\0';
    auth_pgp_decrypt[0] = '\0';
    auth_pgp_entity[0] = '\0';
#endif

    if(!(cfg = fopen(server_confname,"r"))) {
        fprintf(errors,"httpd: could not open server config. file %s\n",server_confname);
        perror("fopen");
        exit(1);
    }
    /* Parse server config file. Remind me to learn yacc. */
    while(!(cfg_getline(l,MAX_STRING_LEN,cfg))) {
        ++n;
        if((l[0] != '#') && (l[0] != '\0')) {
            cfg_getword(w,l);
            
            if(!strcasecmp(w,"ServerType")) {
                if(!strcasecmp(l,"inetd")) standalone=0;
                else if(!strcasecmp(l,"standalone")) standalone=1;
                else {
                    fprintf(errors,"Syntax error on line %d of %s:\n",n,server_confname);
                    fprintf(errors,"ServerType is either inetd or standalone.\n");
                    exit(1);
                }
            }
            else if(!strcasecmp(w,"Port")) {
                cfg_getword(w,l);
                port = atoi(w);
            }
            else if(!strcasecmp(w,"User")) {
                cfg_getword(w,l);
                user_id = uname2id(w);
            } 
            else if(!strcasecmp(w,"Group")) {
                cfg_getword(w,l);
                group_id = gname2id(w);
            }
            else if(!strcasecmp(w,"ServerAdmin")) {
                cfg_getword(w,l);
                strcpy(server_admin,w);
            }
            else if(!strcasecmp(w,"ServerName")) {
                cfg_getword(w,l);
                if(server_hostname)
                    free(server_hostname);
                if(!(server_hostname = strdup(w)))
                    die(NO_MEMORY,"process_resource_config",errors);
            }
            else if(!strcasecmp(w,"ServerRoot")) {
                cfg_getword(w,l);
                if(!is_directory(w)) {
                    fprintf(errors,"Syntax error on line %d of %s:\n",n,server_confname);
                    fprintf(errors,"%s is not a valid directory.\n",w);
                    exit(1);
                }
                strcpy(server_root,w);
	      make_full_path(server_root,DEFAULT_ERRORLOG,error_fname);
	      make_full_path(server_root,DEFAULT_XFERLOG,xfer_fname);
	      make_full_path(server_root,DEFAULT_PIDLOG,pid_fname);
	      make_full_path(server_root,RESOURCE_CONFIG_FILE,srm_confname);
	      make_full_path(server_root,ACCESS_CONFIG_FILE,access_confname);
	      make_full_path(server_root,TYPES_CONFIG_FILE,types_confname);
            }
            else if(!strcasecmp(w,"ErrorLog")) {
                cfg_getword(w,l);
                if(w[0] != '/')
                    make_full_path(server_root,w,error_fname);
                else 
                    strcpy(error_fname,w);
            } 
            else if(!strcasecmp(w,"TransferLog")) {
                cfg_getword(w,l);
                if(w[0] != '/')
                    make_full_path(server_root,w,xfer_fname);
                else strcpy(xfer_fname,w);
            }
            else if(!strcasecmp(w,"PidFile")) {
                cfg_getword(w,l);
                if(w[0] != '/')
                    make_full_path(server_root,w,pid_fname);
                else strcpy(pid_fname,w);
            }
            else if(!strcasecmp(w,"AccessConfig")) {
                cfg_getword(w,l);
                if(w[0] != '/')
                    make_full_path(server_root,w,access_confname);
                else strcpy(access_confname,w);
            }
            else if(!strcasecmp(w,"ResourceConfig")) {
                cfg_getword(w,l);
                if(w[0] != '/')
                    make_full_path(server_root,w,srm_confname);
                else strcpy(srm_confname,w);
            }
            else if(!strcasecmp(w,"TypesConfig")) {
                cfg_getword(w,l);
                if(w[0] != '/')
                    make_full_path(server_root,w,types_confname);
                else strcpy(types_confname,w);
            }
            else if(!strcasecmp(w,"Timeout"))
                timeout = atoi(l);
            else if(!strcasecmp(w,"IdentityCheck")) {
                cfg_getword(w,l);
                if(!strcasecmp(w,"on"))
                    do_rfc931 = 1;
                else if(!strcasecmp(w,"off"))
                    do_rfc931 = 0;
                else {
                    fprintf(errors,"Syntax error on line %d of %s:\n",n,
                            srm_confname);
                    fprintf(errors,"IdentityCheck must be on or off.\n");
                }
            }
#ifdef PEM_AUTH
            else if(!strcasecmp(w,"PEMEncryptCmd")) {
                cfg_getword(w,l);
                strcpy(auth_pem_encrypt,w);
            }
            else if(!strcasecmp(w,"PEMDecryptCmd")) {
                cfg_getword(w,l);
                strcpy(auth_pem_decrypt,w);
            }
            else if(!strcasecmp(w,"PEMServerEntity")) {
                cfg_getword(w,l);
                strcpy(auth_pem_entity,w);
            }
            else if(!strcasecmp(w,"PGPEncryptCmd")) {
                cfg_getword(w,l);
                strcpy(auth_pgp_encrypt,w);
            }
            else if(!strcasecmp(w,"PGPDecryptCmd")) {
                cfg_getword(w,l);
                strcpy(auth_pgp_decrypt,w);
            }
            else if(!strcasecmp(w,"PGPServerEntity")) {
                cfg_getword(w,l);
                strcpy(auth_pgp_entity,w);
            }
#endif
            else {
                fprintf(errors,"Syntax error on line %d of %s:\n",n,server_confname);
                fprintf(errors,"Unknown keyword %s.\n",w);
                exit(1);
            }
        }
    }
    fclose(cfg);
}

/* Document config globals */
char user_dir[MAX_STRING_LEN];
char index_name[MAX_STRING_LEN];
char access_name[MAX_STRING_LEN];
char document_root[MAX_STRING_LEN];
char default_type[MAX_STRING_LEN];
char default_icon[MAX_STRING_LEN];

void process_resource_config(FILE *errors) {
    FILE *cfg;
    char l[MAX_STRING_LEN],w[MAX_STRING_LEN];
    int n=0;

    strcpy(user_dir,DEFAULT_USER_DIR);
    strcpy(index_name,DEFAULT_INDEX);
    strcpy(access_name,DEFAULT_ACCESS_FNAME);
    strcpy(document_root,DOCUMENT_LOCATION);
    strcpy(default_type,DEFAULT_TYPE);
    default_icon[0] = '\0';

    add_opts_int(0,"/",errors);

    if(!(cfg = fopen(srm_confname,"r"))) {
        fprintf(errors,"httpd: could not open document config. file %s\n",
                srm_confname);
        perror("fopen");
        exit(1);
    }

    while(!(cfg_getline(l,MAX_STRING_LEN,cfg))) {
        ++n;
        if((l[0] != '#') && (l[0] != '\0')) {
            cfg_getword(w,l);
            
            if(!strcasecmp(w,"ScriptAlias")) {
                char w2[MAX_STRING_LEN];
            
                cfg_getword(w,l);
                cfg_getword(w2,l);
                if((w[0] == '\0') || (w2[0] == '\0')) {
                    fprintf(errors,"Syntax error on line %d of %s:\n",n,
                            srm_confname);
                    fprintf(errors,
"ScriptAlias must be followed by a fakename, one space, then a realname.\n");
                    exit(1);
                }                
                add_alias(w,w2,SCRIPT_CGI);
            }
            else if(!strcasecmp(w,"OldScriptAlias")) {
                char w2[MAX_STRING_LEN];
            
                cfg_getword(w,l);
                cfg_getword(w2,l);
                if((w[0] == '\0') || (w2[0] == '\0')) {
                    fprintf(errors,"Syntax error on line %d of %s:\n",n,
                            srm_confname);
                    fprintf(errors,
"ScriptAlias must be followed by a fakename, one space, then a realname.\n");
                    exit(1);
                }                
                add_alias(w,w2,SCRIPT_NCSA);
            }
            else if(!strcasecmp(w,"UserDir")) {
                cfg_getword(w,l);
                if(!strcmp(w,"DISABLED"))
                    user_dir[0] = '\0';
                else
                    strcpy(user_dir,w);
            }
            else if(!strcasecmp(w,"DirectoryIndex")) {
                cfg_getword(w,l);
                strcpy(index_name,w);
            } 
            else if(!strcasecmp(w,"DefaultType")) {
                cfg_getword(w,l);
                strcpy(default_type,w);
            }
            else if(!strcasecmp(w,"AccessFileName")) {
                cfg_getword(w,l);
                strcpy(access_name,w);
            } 
            else if(!strcasecmp(w,"DocumentRoot")) {
                cfg_getword(w,l);
                if(!is_directory(w)) {
                    fprintf(errors,"Syntax error on line %d of %s:\n",n,
                            srm_confname);
                    fprintf(errors,"%s is not a valid directory.\n",w);
                    exit(1);
                }
                strcpy(document_root,w);
            } 
            else if(!strcasecmp(w,"Alias")) {
                char w2[MAX_STRING_LEN];
        
                cfg_getword(w,l);
                cfg_getword(w2,l);
                if((w[0] == '\0') || (w2[0] == '\0')) {
                    fprintf(errors,"Syntax error on line %d of %s:\n",n,
                            srm_confname);
                    fprintf(errors,
"Alias must be followed by a fakename, one space, then a realname.\n");
                    exit(1);
                }                
                add_alias(w,w2,STD_DOCUMENT);
            }
            else if(!strcasecmp(w,"AddType")) {
                char w2[MAX_STRING_LEN];
                cfg_getword(w,l);
                cfg_getword(w2,l);
                if((w[0] == '\0') || (w2[0] == '\0')) {
                    fprintf(errors,"Syntax error on line %d of %s:\n",n,
                            srm_confname);
                    fprintf(errors,
"AddType must be followed by a type, one space, then a file or extension.\n");
                    exit(1);
                }
                add_type(w2,w,errors);
            }
            else if(!strcasecmp(w,"AddEncoding")) {
                char w2[MAX_STRING_LEN];
                cfg_getword(w,l);
                cfg_getword(w2,l);
                if((w[0] == '\0') || (w2[0] == '\0')) {
                    fprintf(errors,"Syntax error on line %d of %s:\n",n,
                            srm_confname);
                    fprintf(errors,
"AddEncoding must be followed by a type, one space, then a file or extension.\n");
                    exit(1);
                }
                add_encoding(w2,w,errors);
            }
            else if(!strcasecmp(w,"Redirect")) {
                char w2[MAX_STRING_LEN];
                cfg_getword(w,l);
                cfg_getword(w2,l);
                if((w[0] == '\0') || (w2[0] == '\0') || (!is_url(w2))) {
                    fprintf(errors,"Syntax error on line %d of %s:\n",n,
                            srm_confname);
                    fprintf(errors,
"Redirect must be followed by a document, one space, then a URL.\n");
                    exit(1);
                }
                add_redirect(w,w2);
            }
            else if(!strcasecmp(w,"FancyIndexing")) {
                cfg_getword(w,l);
                if(!strcmp(w,"on"))
                    add_opts_int(FANCY_INDEXING,"/",errors);
                else if(!strcmp(w,"off"))
                    add_opts_int(0,"/",errors);
                else {
                    fprintf(errors,"Syntax error on line %d of %s:\n",n,
                            srm_confname);
                    fprintf(errors,"FancyIndexing must be on or off.\n");
                    exit(1);
                }
            }
            else if(!strcasecmp(w,"AddDescription")) {
                char desc[MAX_STRING_LEN];
                int fq;
                if((fq = ind(l,'\"')) == -1) {
                    fprintf(errors,"Syntax error on line %d of %s:\n",n,
                            srm_confname);
fprintf(errors,"AddDescription must have quotes around the description.\n");
                    exit(1);
                }
                else {
                    getword(desc,&l[++fq],'\"');
                    cfg_getword(w,&l[fq]);
                    add_desc(BY_PATH,desc,w,"/",errors);
                }
            }
            else if(!strcasecmp(w,"IndexIgnore")) {
                while(l[0]) {
                    cfg_getword(w,l);
                    add_ignore(w,"/",errors);
                }
            }
            else if(!strcasecmp(w,"AddIcon")) {
                char w2[MAX_STRING_LEN];
                cfg_getword(w2,l);
                while(l[0]) {
                    cfg_getword(w,l);
                    add_icon(BY_PATH,w2,w,"/",errors);
                }
            }
            else if(!strcasecmp(w,"AddIconByType")) {
                char w2[MAX_STRING_LEN];
                cfg_getword(w2,l);
                while(l[0]) {
                    cfg_getword(w,l);
                    add_icon(BY_TYPE,w2,w,"/",errors);
                }
            }
            else if(!strcasecmp(w,"AddIconByEncoding")) {
                char w2[MAX_STRING_LEN];
                cfg_getword(w2,l);
                while(l[0]) {
                    cfg_getword(w,l);
                    add_icon(BY_ENCODING,w2,w,"/",errors);
                }
            }
            else if(!strcasecmp(w,"AddAlt")) {
                char w2[MAX_STRING_LEN];
                cfg_getword(w2,l);
                while(l[0]) {
                    cfg_getword(w,l);
                    add_alt(BY_PATH,w2,w,"/",errors);
                }
            }
            else if(!strcasecmp(w,"AddAltByType")) {
                char w2[MAX_STRING_LEN];
                cfg_getword(w2,l);
                while(l[0]) {
                    cfg_getword(w,l);
                    add_alt(BY_TYPE,w2,w,"/",errors);
                }
            }
            else if(!strcasecmp(w,"AddAltByEncoding")) {
                char w2[MAX_STRING_LEN];
                cfg_getword(w2,l);
                while(l[0]) {
                    cfg_getword(w,l);
                    add_alt(BY_ENCODING,w2,w,"/",errors);
                }
            }
            else if(!strcasecmp(w,"DefaultIcon")) {
                cfg_getword(w,l);
                strcpy(default_icon,w);
            }
            else if(!strcasecmp(w,"ReadmeName")) {
                cfg_getword(w,l);
                add_readme(w,"/",errors);
            }
            else if(!strcasecmp(w,"HeaderName")) {
                cfg_getword(w,l);
                add_header(w,"/",errors);
            }
            else if(!strcasecmp(w,"IndexOptions"))
                add_opts(l,"/",errors);
            else {
                fprintf(errors,"Syntax error on line %d of %s:\n",n,
                        srm_confname);
                fprintf(errors,"Unknown keyword %s.\n",w);
                exit(1);
            }
        }
    }
    fclose(cfg);
}


/* Auth Globals */
char *auth_type;
char *auth_name;
char *auth_pwfile;
char *auth_grpfile;

/* Access Globals*/
int num_sec;
security_data sec[MAX_SECURITY];

void access_syntax_error(int n, char *err, char *file, FILE *out) {
    if(!file) {
        fprintf(out,"Syntax error on line %d of access config. file.\n",n);
        fprintf(out,"%s\n",err);
        exit(1);
    }
    else {
        char e[MAX_STRING_LEN];
        sprintf(e,"httpd: syntax error or override violation in access control file %s, reason: %s",file,err);
        die(SERVER_ERROR,e,out);
    }
}

int parse_access_dir(FILE *f, int line, char or, char *dir, 
                     char *file, FILE *out) 
{
    char l[MAX_STRING_LEN];
    char t[MAX_STRING_LEN];
    char w[MAX_STRING_LEN];
    char w2[MAX_STRING_LEN];
    int n=line;
    register int x,i;

    x = num_sec;

    sec[x].opts=OPT_UNSET;
    sec[x].override = or;
    if(!(sec[x].d = (char *)malloc((sizeof(char)) * (strlen(dir) + 2))))
        die(NO_MEMORY,"parse_access_dir",out);
    if(is_matchexp(dir))
        strcpy(sec[x].d,dir);
    else
        strcpy_dir(sec[x].d,dir);

    sec[x].auth_type = NULL;
    sec[x].auth_name = NULL;
    sec[x].auth_pwfile = NULL;
    sec[x].auth_grpfile = NULL;
    for(i=0;i<METHODS;i++) {
        sec[x].order[i] = DENY_THEN_ALLOW;
        sec[x].num_allow[i]=0;
        sec[x].num_deny[i]=0;
        sec[x].num_auth[i] = 0;
    }

    while(!(cfg_getline(l,MAX_STRING_LEN,f))) {
        ++n;
        if((l[0] == '#') || (!l[0])) continue;
        cfg_getword(w,l);

        if(!strcasecmp(w,"AllowOverride")) {
            if(file)
                access_syntax_error(n,"override violation",file,out);
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
                else if(!strcasecmp(w,"None"))
                    sec[x].override = OR_NONE;
                else if(!strcasecmp(w,"All")) 
                    sec[x].override = OR_ALL;
                else {
                    access_syntax_error(n,
"Unknown keyword in AllowOverride directive.",file,out);
                }
            }
        } 
        else if(!strcasecmp(w,"Options")) {
            if(!(or & OR_OPTIONS))
                access_syntax_error(n,"override violation",file,out);
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
                    access_syntax_error(n,
"Unknown keyword in Options directive.",file,out);
                }
            }
        }
        else if(!strcasecmp(w,"AuthName")) {
            if(!(or & OR_AUTHCFG))
                access_syntax_error(n,"override violation",file,out);
            if(sec[x].auth_name) 
                free(sec[x].auth_name);
            if(!(sec[x].auth_name = strdup(l)))
                die(NO_MEMORY,"parse_access_dir",out);
        }
        else if(!strcasecmp(w,"AuthType")) {
            if(!(or & OR_AUTHCFG))
                access_syntax_error(n,"override violation",file,out);
            cfg_getword(w,l);
            if(sec[x].auth_type) 
                free(sec[x].auth_type);
            if(!(sec[x].auth_type = strdup(w)))
                die(NO_MEMORY,"parse_access_dir",out);
        }
        else if(!strcasecmp(w,"AuthUserFile")) {
            if(!(or & OR_AUTHCFG))
                access_syntax_error(n,"override violation",file,out);
            cfg_getword(w,l);
            if(sec[x].auth_pwfile) 
                free(sec[x].auth_pwfile);
            if(!(sec[x].auth_pwfile = strdup(w)))
                die(NO_MEMORY,"parse_access_dir",out);
        }
        else if(!strcasecmp(w,"AuthGroupFile")) {
            if(!(or & OR_AUTHCFG))
                access_syntax_error(n,"override violation",file,out);
            cfg_getword(w,l);
            if(sec[x].auth_grpfile) 
                free(sec[x].auth_grpfile);
            if(!(sec[x].auth_grpfile = strdup(w)))
                die(NO_MEMORY,"parse_access_dir",out);
        }
        else if(!strcasecmp(w,"AddType")) {
            if(!(or & OR_FILEINFO))
                access_syntax_error(n,"override violation",file,out);
            cfg_getword(w,l);
            cfg_getword(w2,l);
            if((w[0] == '\0') || (w2[0] == '\0')) {
                access_syntax_error(n,
"AddType must be followed by a type, one space, then a file or extension.",
                                    file,out);
            }
            add_type(w2,w,out);
        }
        else if(!strcasecmp(w,"DefaultType")) {
            if(!(or & OR_FILEINFO))
                access_syntax_error(n,"override violation",file,out);
            cfg_getword(w,l);
            strcpy(default_type,w);
        }
        else if(!strcasecmp(w,"AddEncoding")) {
            if(!(or & OR_FILEINFO))
                access_syntax_error(n,"override violation",file,out);
            cfg_getword(w,l);
            cfg_getword(w2,l);
            if((w[0] == '\0') || (w2[0] == '\0')) {
                access_syntax_error(n,
"AddEncoding must be followed by a type, one space, then a file or extension.",
                                    file,out);
            }
            add_encoding(w2,w,out);
        }
        else if(!strcasecmp(w,"DefaultIcon")) {
            if(!(or & OR_INDEXES))
                access_syntax_error(n,"override violation",file,out);
            cfg_getword(w,l);
            strcpy(default_icon,w);
        }
        else if(!strcasecmp(w,"AddDescription")) {
            char desc[MAX_STRING_LEN];
            int fq;
            
            if(!(or & OR_INDEXES))
                access_syntax_error(n,"override violation",file,out);
            if((fq = ind(l,'\"')) == -1)
                access_syntax_error(n,"AddDescription must have quotes",
                                    file,out);
            else {
                getword(desc,&l[++fq],'\"');
                cfg_getword(w,&l[fq]);
                add_desc(BY_PATH,desc,w,sec[x].d,out);
            }
        }
        else if(!strcasecmp(w,"IndexIgnore")) {
            if(!(or & OR_INDEXES))
                access_syntax_error(n,"override violation",file,out);
            while(l[0]) {
                cfg_getword(w,l);
                add_ignore(w,sec[x].d,out);
            }
        }
        else if(!strcasecmp(w,"AddIcon")) {
            char w2[MAX_STRING_LEN];
            
            if(!(or & OR_INDEXES))
                access_syntax_error(n,"override violation",file,out);
            cfg_getword(w2,l);
            while(l[0]) {
                cfg_getword(w,l);
                add_icon(BY_PATH,w2,w,sec[x].d,out);
            }
        }
        else if(!strcasecmp(w,"AddIconByType")) {
            char w2[MAX_STRING_LEN];
            
            if(!(or & OR_INDEXES))
                access_syntax_error(n,"override violation",file,out);
            cfg_getword(w2,l);
            while(l[0]) {
                cfg_getword(w,l);
                add_icon(BY_TYPE,w2,w,sec[x].d,out);
            }
        }
        else if(!strcasecmp(w,"AddIconByEncoding")) {
            char w2[MAX_STRING_LEN];
            
            if(!(or & OR_INDEXES))
                access_syntax_error(n,"override violation",file,out);
            cfg_getword(w2,l);
            while(l[0]) {
                cfg_getword(w,l);
                add_icon(BY_ENCODING,w2,w,sec[x].d,out);
            }
        }
        else if(!strcasecmp(w,"ReadmeName")) {
            if(!(or & OR_INDEXES))
                access_syntax_error(n,"override violation",file,out);
            cfg_getword(w,l);
            add_readme(w,sec[x].d,out);
        }
        else if(!strcasecmp(w,"HeaderName")) {
            if(!(or & OR_INDEXES))
                access_syntax_error(n,"override violation",file,out);
            cfg_getword(w,l);
            add_header(w,sec[x].d,out);
        }
        else if(!strcasecmp(w,"IndexOptions")) {
            if(!(or & OR_INDEXES))
                access_syntax_error(n,"override violation",file,out);
            add_opts(l,sec[x].d,out);
        }
        else if(!strcasecmp(w,"Redirect")) {
            if(!(or & OR_FILEINFO))
                access_syntax_error(n,"override violation",file,out);
            cfg_getword(w,l);
            cfg_getword(w2,l);
            if((w[0] == '\0') || (w2[0] == '\0') || (!is_url(w2))) {
                access_syntax_error(n,
"Redirect must be followed by a document, one space, then a URL.",file,out);
            }
            if(!file)
                add_redirect(w,w2);
            else
                access_syntax_error(n,
"Redirect no longer supported from .htaccess files.",file,out);
        }
        else if(!strcasecmp(w,"<Limit")) {
            int m[METHODS];

            if(!(or & OR_LIMIT))
                access_syntax_error(n,"override violation",file,out);
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
                    access_syntax_error(n,"Limit missing /Limit",file,out);
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
                        access_syntax_error(n,"Unknown order.",file,out);
                } 
                else if((!strcasecmp(w,"allow"))) {
                    cfg_getword(w,l);
                    if(strcmp(w,"from"))
                        access_syntax_error(n,
                                            "allow must be followed by from.",
                                            file,out);
                    while(1) {
                        cfg_getword(w,l);
                        if(!w[0]) break;
                        for(i=0;i<METHODS;i++)
                            if(m[i]) {
                                int q=sec[x].num_allow[i]++;
                                if(!(sec[x].allow[i][q] = strdup(w)))
                                    die(NO_MEMORY,"parse_access_dir",out);
                            }
                    }
                }
                else if(!strcasecmp(w,"require")) {
                    for(i=0;i<METHODS;i++)
                        if(m[i]) {
                            int q=sec[x].num_auth[i]++;
                            if(!(sec[x].auth[i][q] = strdup(l)))
                                die(NO_MEMORY,"parse_access_dir",out);
                        }
                }
                else if((!strcasecmp(w,"deny"))) {
                    cfg_getword(w,l);
                    if(strcmp(w,"from"))
                        access_syntax_error(n,
                                            "deny must be followed by from.",
                                            file,out);
                    while(1) {
                        cfg_getword(w,l);
                        if(!w[0]) break;
                        for(i=0;i<METHODS;i++)
                            if(m[i]) {
                                int q=sec[x].num_deny[i]++;
                                if(!(sec[x].deny[i][q] = strdup(w)))
                                    die(NO_MEMORY,"parse_access_dir",out);
                            }
                    }
                }
                else
                    access_syntax_error(n,"Unknown keyword in Limit region.",
                                        file,out);
            }
        }
        else if(!strcasecmp(w,"</Directory>"))
            break;
        else {
            char errstr[MAX_STRING_LEN];
            sprintf(errstr,"Unknown method %s",w);
            access_syntax_error(n,errstr,file,out);
            return;
        }
    }
    ++num_sec;
    return n;
}


void parse_htaccess(char *path, char override, FILE *out) {
    FILE *f;
    char t[MAX_STRING_LEN];
    char d[MAX_STRING_LEN];
    int x;

    strcpy(d,path);
    make_full_path(d,access_name,t);

    if((f=fopen(t,"r"))) {
        parse_access_dir(f,-1,override,d,t,out);
        fclose(f);
    }
}


void process_access_config(FILE *errors) {
    FILE *f;
    char l[MAX_STRING_LEN];
    char w[MAX_STRING_LEN];
    int n;

    num_sec = 0;n=0;
    if(!(f=fopen(access_confname,"r"))) {
        fprintf(errors,"httpd: could not open access configuration file %s.\n",
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
        n=parse_access_dir(f,n,OR_ALL,w,NULL,errors);
    }
    fclose(f);
}

int get_pw(char *user, char *pw, FILE *errors) {
    FILE *f;
    char errstr[MAX_STRING_LEN];
    char l[MAX_STRING_LEN];
    char w[MAX_STRING_LEN];

    if(!(f=fopen(auth_pwfile,"r"))) {
        sprintf(errstr,"Could not open user file %s",auth_pwfile);
        die(SERVER_ERROR,errstr,errors);
    }
    while(!(cfg_getline(l,MAX_STRING_LEN,f))) {
        if((l[0] == '#') || (!l[0])) continue;
        getword(w,l,':');

        if(!strcmp(user,w)) {
            strcpy(pw,l);
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}


struct ge {
    char *name;
    char *members;
    struct ge *next;
};

static struct ge *grps;

int init_group(char *grpfile, FILE *out) {
    FILE *f;
    struct ge *p;
    char l[MAX_STRING_LEN],w[MAX_STRING_LEN];

    if(!(f=fopen(grpfile,"r")))
        return 0;

    grps = NULL;
    while(!(cfg_getline(l,MAX_STRING_LEN,f))) {
        if((l[0] == '#') || (!l[0])) continue;
        getword(w,l,':');
        if(!(p = (struct ge *) malloc (sizeof(struct ge))))
            die(NO_MEMORY,"init_group",out);
        if(!(p->name = strdup(w)))
            die(NO_MEMORY,"init_group",out);
        if(!(p->members = strdup(l)))
            die(NO_MEMORY,"init_group",out);
        p->next = grps;
        grps = p;
    }
    fclose(f);
    return 1;
}

int in_group(char *user, char *group) {
    struct ge *p = grps;
    char l[MAX_STRING_LEN],w[MAX_STRING_LEN];

    while(p) {
        if(!strcmp(p->name,group)) {
            strcpy(l,p->members);
            while(l[0]) {
                getword(w,l,' ');
                if(!strcmp(w,user))
                    return 1;
            }
        }
        p=p->next;
    }
    return 0;
}

void kill_group() {
    struct ge *p = grps, *q;

    while(p) {
        free(p->name);
        free(p->members);
        q=p;
        p=p->next;
        free(q);
    }   
}

void read_config(FILE *errors)
{
    reset_aliases();
    process_server_config(errors);
    init_mime(errors);
    init_indexing();
    process_resource_config(errors);
    process_access_config(errors);
}
