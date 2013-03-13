/*
 * http_request.c: functions to get and process requests
 * 
 * Rob McCool 3/21/93
 * 
 */


#include "httpd.h"
#ifdef LOGGING
#include <arpa/inet.h>  /* for inet_ntoa */
#include <time.h>  /* for ctime */
#endif /* LOGGING */

#ifdef ANNOTATIONS
#define COMMANDS 5
#else
#define COMMANDS 1
#endif

static char *commands[]={
    "GET ",
#ifdef ANNOTATIONS
    "ANN_GET ",
    "ANN_SET ",
    "ANN_CHANGE ",
    "ANN_DELETE ",
#endif
    };

#ifdef OLD
void send_fd(FILE *f, FILE *fd)
{
    int num_chars=0;
    
    while(!feof(f)) {
        char c;
        
        c=fgetc(f);

#ifdef TRUNCATE_LEN
        ++num_chars;
        
        if((c==LINEFEED) || (c==CRETURN) || (num_chars==TRUNCATE_LEN)) {
            fputc(LINEFEED,fd);
            
            if(c==CRETURN && !feof(f)) {
                ++num_chars;
                c=fgetc(f);
                if(c!=LINEFEED)
                    fputc(c,fd);
            }
            else if(num_chars==TRUNCATE_LEN) {
                while((c!=LINEFEED) && !feof(f))
                    c=fgetc(f);
            }
            num_chars=0;
        }
        else 
#endif
            fputc(c,fd);
    }
}
#endif

void send_fd(FILE *f, FILE *fd)
{
    int num_chars=0;

    while (1)
      {
        char c;
        
        c = fgetc(f);
        if (feof(f))
          goto done;

#ifdef TRUNCATE_LEN
        ++num_chars;
        
        if((c==LINEFEED) || (c==CRETURN) || (num_chars==TRUNCATE_LEN)) {
          fputc(LINEFEED,fd);
          
          if(c==CRETURN && !feof(f)) {
            ++num_chars;
            c=fgetc(f);
            if(c!=LINEFEED)
              fputc(c,fd);
          }
          else if(num_chars==TRUNCATE_LEN) {
            while((c!=LINEFEED) && !feof(f))
              c=fgetc(f);
          }
          num_chars=0;
        }
        else 
#endif
          fputc(c,fd);
    }
    
  done:
    return;
}

void send_file(char *file, FILE *fd) {
    FILE *f;

    if(!(f=fopen(file,"r")))
         client_error(fd,BAD_FILE);

    send_fd(f,fd);
}

void send_error_file(char *file,FILE *fd) {
    FILE *f;

    if(!(f=fopen(file,"r"))) {
        fprintf(fd,"<TITLE>Bad configuration</TITLE>%c",LINEFEED);
        fprintf(fd,"<H1>Bad configuration</H1>%c",LINEFEED);
        fprintf(fd,
                "%cThis server cannot find an error file to send you.<P>%c",
                LINEFEED,LINEFEED);
        exit(1);
    }
    send_fd(f,fd); 
}


/* GET */
void send_node(char *file,FILE *fd)
{
    struct stat finfo;

    if(stat(file,&finfo) == -1)
        client_error(fd,BAD_FILE);

    if(S_ISDIR(finfo.st_mode)) {
        char name[MAX_STRING_LEN];

        strcpy(name,file);
        /* Bugfix in case there's no trailing slash. */
        if (name[strlen(name)-1] != '/')
          strcat(name, "/");
        strcat(name,HTML_DIR_CONTENT);
        if(stat(name,&finfo) == -1)
            /* index file not found; get dir. list and send it back as HTML */
            index_directory(file,fd);
        else send_file(name,fd);
    }

    else if(S_ISREG(finfo.st_mode))
        send_file(file,fd);

    else client_error(fd,BAD_FILE);
}



void process_request(FILE *in, FILE *out)
{
    char cmd_line[MAX_STRING_LEN],*ret;
    register int x,y,z;
#ifdef LOGGING
    char *who_called = NULL;
    struct sockaddr addr;
    int len, retval;
    FILE *fp;
    time_t time_val = time(NULL);
    char *time_string = ctime(&time_val);
#endif /* LOGGING */

    for (x=0; x<MAX_STRING_LEN; x++)
    {
	cmd_line[x] = (char)fgetc(in);
	if ((cmd_line[x] == ' ')||(cmd_line[x] == LINEFEED))
	{
	    break;
	}
    }
    if ((x == MAX_STRING_LEN)||(cmd_line[x] == LINEFEED))
    {
        client_error(out,BAD_REQUEST);
    }
    cmd_line[x + 1] = '\0';

/* Currently, this does not take into account execution of different cmds */
    for(y=0;y<COMMANDS;y++) {
        if(!(strncmp(commands[y],cmd_line,strlen(commands[y])))) {
	    int val;

	    val = fgetc(in);
	    if (val != '/')
	    {
                client_error(out,BAD_FILE); /* FIX */
	    }

#ifdef ANNOTATIONS
            switch(y) {
		case 0: /* GET */
			break;
		case 1: /* ANN_GET */
			Get_request(in, out);
			return;
			break;
		case 2: /* ANN_SET */
			Set_request(in, out);
			return;
			break;
		case 3: /* ANN_CHANGE */
			Change_request(in, out);
			return;
			break;
		case 4: /* ANN_DELETE */
			Delete_request(in, out);
			return;
			break;
            }
#endif

	    if(!(ret=(char *)malloc(MAX_STRING_LEN*sizeof(char))))
		server_error(out,MEMORY);

	    x++;
	    cmd_line[x] = '/';
	    z = x;
	    x++;
	    val = fgetc(in);
	    while ((x < (MAX_STRING_LEN - 1))&&(val != EOF)&&
			(val != LINEFEED)&&(val != CRETURN)&&(!isspace(val)))
	    {
		cmd_line[x] = (char)val;
		x++;
		val = fgetc(in);
	    }
	    cmd_line[x] = '\0';

#ifdef LOGGING
	/*
	 * Get a sockaddr structure filled in for our peer.
	 */
	len = sizeof(struct sockaddr);
	retval = getpeername(fileno(stdin), &addr, &len);
	if (retval == 0)
	{
		struct in_addr *iaddr;
		struct hostent *hptr;

		iaddr = &(((struct sockaddr_in *)&addr)->sin_addr);
		hptr = gethostbyaddr((char *)iaddr,
			sizeof(struct in_addr), AF_INET);
		if (hptr != NULL)
		{
			who_called = (char *)malloc(strlen(hptr->h_name) + 1);
			strcpy(who_called, hptr->h_name);
		}
		else
		{
			char *iname;

			iname = inet_ntoa(*iaddr);
			if (iname != NULL)
			{
				who_called = (char *)malloc(strlen(iname) + 1);
				strcpy(who_called, iname);
			}
			else
			{
				who_called = (char *)malloc(
					strlen("UNKNOWN_CALLER") + 1);
				strcpy(who_called, "UNKNOWN_CALLER");
			}
		}
	}
	else
	{
		who_called = (char *)malloc(strlen("UNKNOWN_CALLER") + 1);
		strcpy(who_called, "UNKNOWN_CALLER");
	}

	fp = fopen(LOGFILE, "a");
        if (fp)
          {
            time_string[strlen(time_string) - 1] = '\0';
            fprintf(fp, "%s\t[%s] %s\n", who_called, time_string, cmd_line);
            fclose(fp);
          }
#endif /* LOGGING */

            strcpy_nocrlf(ret, &cmd_line[z]);
            getparents(ret);
            translate_name(ret);
            switch(y) {
              case 0: /* GET */
                if(ret[0]!='/') client_error(out,BAD_FILE); /* FIX */
                send_node(ret,out);
                break;
            }
            return;
        }
    }
    client_error(out,BAD_REQUEST);
}

