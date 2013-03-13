/*
 * ann_request.c: functions to get and process annotation requests
 * 
 * Eric Bina
 * 
 */


#include "httpd.h"
#include <time.h>


#ifdef ANNOTATIONS

/*
 * All the recognized arguments that you can pass in an annotation
 * request.
 */
static char *ArgUrl, *ArgGroup, *ArgUser, *ArgTitle, *ArgDate, *ArgAudio;
static int ArgLength;



#define HASH_TABLE_SIZE 1037


/*
 * Hash a url to a number used to name a log file.
 */
static int HashUrl (char *url)
{
	int len, i, val;

	if (!url)
		return 0;
	len = strlen (url);
	if (!len)
		return 0;
	val = 0;
	for (i = 0; i < 10; i++)
		val += url[(i * val + 7) % len];

	return val % HASH_TABLE_SIZE;
}


#define BUFF_SIZE 8192

/*
 * Function to read from the passed FILE * until the characer
 * cstop is reached, EOF, or LINEFEED.  Return the length of
 * the value read (not counting the trailing '\0') in sz.
 * If escapes is 1, backslash escapes the character immediately after it.
 */
char *readto(FILE *in, char cstop, int *sz, int escapes)
{
	char *buf;
	int i, size;

	*sz = 0;
	buf = (char *)malloc(BUFF_SIZE * sizeof(char));
	if (buf == NULL)
	{
		return(NULL);
	}
	for (i=0; i<BUFF_SIZE; i++)
	{
		int val;

		val = fgetc(in);
		if ((escapes)&&(val == '\\'))
		{
			val = fgetc(in);
		}
		else if (val == cstop)
		{
			buf[i] = '\0';
			break;
		}
		else if (val == LINEFEED)
		{
			return(NULL);
		}
		buf[i] = (char)val;
	}
	size = i;
	while (i == BUFF_SIZE)
	{
		char *tptr;

		tptr = buf;
		buf = (char *)malloc((size + BUFF_SIZE) * sizeof(char));
		if (buf == NULL)
		{
		    return(NULL);
		}
		bcopy(tptr, buf, size);
		free(tptr);
		tptr = (char *)(buf + size);
		for (i=0; i<BUFF_SIZE; i++)
		{
			int val;

			val = fgetc(in);
			if ((escapes)&&(val == '\\'))
			{
				val = fgetc(in);
			}
			else if (val == cstop)
			{
				tptr[i] = '\0';
				break;
			}
			else if (val == LINEFEED)
			{
				return(NULL);
			}
			tptr[i] = (char)val;
		}
		size += i;
	}
	*sz = size;
	return(buf);
}


/*
 * Read the next value from the annotation request input stream.
 * Values are everything from the '=' to the ';', unless the first
 * character is a '"', in which case it is everything up to the
 * closing quote.
 */
char *read_val(FILE *in)
{
	int val;
	int vsize;
	char *argval;
	char *tptr;

	argval = NULL;
	val = fgetc(in);
	if (val == '\"')
	{
		int junk;

		argval = readto(in, '\"', &vsize, 1);
		if (argval == NULL)
		{
			return(NULL);
		}
		tptr = readto(in, ';', &junk, 0);
		free(tptr);
	}
	else
	{
		char *tptr;

		tptr = readto(in, ';', &vsize, 0);
		if (tptr == NULL)
		{
			return(NULL);
		}
		argval = (char *)malloc((vsize + 2) * sizeof(char));
		if (argval == NULL)
		{
			return(NULL);
		}
		argval[0] = (char)val;
		strcpy(&argval[1], tptr);
		free(tptr);
		vsize++;
	}

	return(argval);
}


/*
 * Process the annotation request input stream, and set the values of
 * all the known accepted parameters.
 */
void ReadArgs(FILE *in, FILE *out)
{
	int ret, size;
	char *arg;
	time_t time_val;
	char *time_string;

	ArgUrl = NULL;
	ArgGroup = NULL;
	ArgUser = NULL;
	ArgTitle = NULL;
	ArgDate = NULL;
	ArgAudio = NULL;
	ArgLength = -1;

	arg = readto(in, '=', &size, 0);
	if (arg == NULL)
	{
		client_error(out, BAD_ANN_REQUEST);
	}
	while (arg[0] != '\0')
	{
		if (strncmp(arg, "url", 3) == 0)
		{
			ArgUrl = read_val(in);
			if (ArgUrl == NULL)
			{
				client_error(out, BAD_ANN_REQUEST);
			}
		}
		else if (strncmp(arg, "group", 5) == 0)
		{
			ArgGroup = read_val(in);
			if (ArgGroup == NULL)
			{
				client_error(out, BAD_ANN_REQUEST);
			}
		}
		else if (strncmp(arg, "user", 4) == 0)
		{
			ArgUser = read_val(in);
			if (ArgUser == NULL)
			{
				client_error(out, BAD_ANN_REQUEST);
			}
		}
		else if (strncmp(arg, "title", 5) == 0)
		{
			ArgTitle = read_val(in);
			if (ArgTitle == NULL)
			{
				client_error(out, BAD_ANN_REQUEST);
			}
		}
		else if (strncmp(arg, "date", 4) == 0)
		{
			ArgDate = read_val(in);
			if (ArgDate == NULL)
			{
				client_error(out, BAD_ANN_REQUEST);
			}
		}
		else if (strncmp(arg, "audio", 5) == 0)
		{
			ArgAudio = read_val(in);
			if (ArgAudio == NULL)
			{
				client_error(out, BAD_ANN_REQUEST);
			}
		}
		else if (strncmp(arg, "length", 6) == 0)
		{
			char *tptr;

			tptr = read_val(in);
			if (tptr == NULL)
			{
				client_error(out, BAD_ANN_REQUEST);
			}
			ArgLength = atoi(tptr);
			free(tptr);
		}
		else
		{
			char *unknown;

			unknown = read_val(in);
			if (unknown == NULL)
			{
				client_error(out, BAD_ANN_REQUEST);
			}
			free(unknown);
		}

		free(arg);
		arg = readto(in, '=', &size, 0);
		if (arg == NULL)
		{
			client_error(out, BAD_ANN_REQUEST);
		}
	}

	/*
	 * We allow the client to pass us the date, but we ignore
	 * it and calculate our own.
	 */
	time_val = time(NULL);
	time_string = ctime(&time_val);
	time_string[strlen(time_string) - 1] = '\0';
	ArgDate = strdup(time_string);
	if (ArgDate == NULL)
	{
		client_error(out, BAD_ANN_REQUEST);
	}
}


/*
 * We know we have received a SET_ANN request.  Process it.
 */
void Set_request(FILE *in, FILE *out)
{
	int indx;
	int ret, hash;
	LogData *lptr;
	char *data;

	/*
	 * Read the args, and make sure we have a URL to
	 * set the annotation on.
	 */
	ReadArgs(in, out);
	if (ArgUrl == NULL)
	{
		client_error(out, BAD_ANN_REQUEST);
	}
/*
fprintf(out, "<PRE>\n");
fprintf(out, "Got a Set-Url-Annotations Request\n");
fprintf(out, "URL = %s\nGroup = %s\nUser = %s\nDate = %s\n",
ArgUrl, ArgGroup, ArgUser, ArgDate);
fprintf(out, "Length = %d\n", ArgLength);
*/
	data = NULL;
	/*
	 * Read the data for this new annotation.
	 */
	if (ArgLength > 0)
	{
		data = (char *)malloc(ArgLength * sizeof(char));
		if (data == NULL)
		{
			client_error(out, BAD_ANN_REQUEST);
		}
		ret = fread(data, sizeof(char), ArgLength, in);
		if (ret != ArgLength)
		{
			client_error(out, BAD_ANN_REQUEST);
		}
	}

	/*
	 * Lock the logfile prepratory to setting the new annotation.
	 */
	hash = HashUrl(ArgUrl);
	lptr = GetLogData(hash);
	if (lptr == NULL)
	{
	    client_error(out, ANN_SERVER_ERROR);
	}
	indx = MaxIndex(lptr);

	/*
	 * If this is an audio annotation, make the real sound file it
	 * points to, and then put in a standard text annotation.
	 */
	if (ArgAudio != NULL)
	{
		char *url;

                /* Looks like ArgAudio is a string corresponding to
                   the type of audio -- we should use this. @@@ */
		url = WriteAudioData(hash, (indx + 1),
			data, ArgLength, ArgAudio, 0);
		if (url == NULL)
		{
			UnlockIt();
			client_error(out, ANN_SERVER_ERROR);
		}
		if (data)
		{
			free((char *)data);
		}
		data = (char *)malloc(strlen("This is an audio annotation. <P>\n\nTo hear the annotation, go <A HREF=\"%s\">here</A>. <P>\n") +
			strlen(url) + 1);
		if (data == NULL)
		{
			UnlockIt();
			client_error(out, ANN_SERVER_ERROR);
		}
		sprintf(data, "This is an audio annotation. <P>\n\nTo hear the annotation, go <A HREF=\"%s\">here</A>. <P>\n", url);
		ArgLength = strlen(data) + 1;
		free(url);
	}

	/*
	 * Write the annotation, modify and write the log.
	 * Unlock the lock, and free memory.
	 */
	ret = WriteAnnData(ArgUrl, hash, (indx + 1),
		ArgTitle, ArgUser, ArgDate, data, ArgLength, 0);
	if (ret < 0)
	{
		UnlockIt();
		client_error(out, ANN_SERVER_ERROR);
	}
	indx = ret;
	ret = AddLogData(lptr, indx, ArgUrl);
	if (ret != 1)
	{
		UnlockIt();
		client_error(out, ANN_SERVER_ERROR);
	}
	ret = WriteLogData(lptr);
	if (ret < 0)
	{
		client_error(out, ANN_SERVER_ERROR);
	}
	if ((ArgLength > 0)&&(data != NULL))
	{
		free(data);
	}
	FreeLogData(lptr);
}


/*
 * We know we have received a GET_ANN request.  Process it.
 */
void Get_request(FILE *in, FILE *out)
{
	int i;
	int ret, hash;
	LogData *lptr;
	int *alist, acnt;

	/*
	 * Read the args, and make sure we have a URL to
	 * get the annotations on.
	 */
	ReadArgs(in, out);
	if (ArgUrl == NULL)
	{
		client_error(out, BAD_ANN_REQUEST);
	}
/*
fprintf(out, "<PLAINTEXT>\n");
fprintf(out, "Got a Get-Url-Annotations Request\n");
fprintf(out, "URL = %s\nGroup = %s\nUser = %s\nDate = %s\n",
ArgUrl, ArgGroup, ArgUser, ArgDate);
fprintf(out, "Length = %d\n", ArgLength);
*/

	/*
	 * Read and lock the log file.
	 */
	hash = HashUrl(ArgUrl);
	lptr = GetLogData(hash);
	if (lptr == NULL)
	{
	    client_error(out, ANN_SERVER_ERROR);
	}

	ret = FindLogData(lptr, ArgUrl, &alist, &acnt);
	/*
	 * If there are annotation on this URL, we need to read each one,
	 * and extract the title, user, and date to make the list
	 * of group annotation to be placed at the end of the
	 * document.
	 */
	if ((ret > 0)&&(acnt > 0))
	{
		char *host;

                host = full_hostname();
		fprintf(out, "<H2>Group Annotations</H2>\n");
		fprintf(out, "<UL>\n");
		for (i=0; i<acnt; i++)
		{
			ret = ReadAnnData(hash, alist[i],
				&ArgTitle, &ArgUser, &ArgDate);
			if (ret != 1)
			{
				UnlockIt();
				client_error(out, ANN_SERVER_ERROR);
			}
			fprintf(out, "<LI> <A HREF=\"http://%s:%d%s/%d-%d.html\">%s</A> (%s)\n",
				host, ANN_PORT, ANN_VIRTUAL_DIR, hash,
				alist[i],
				ArgTitle, ArgDate);
		}
		fprintf(out, "</UL>\n");
	}
	else
	{
	}

	UnlockIt();
}


/*
 * We know we have received a CHANGE_ANN request.  Process it.
 */
void Change_request(FILE *in, FILE *out)
{
	int i, ret, hash;
	LogData *lptr;
	char *file, *data, *ptr;
	char *url;

	/*
	 * Read the args, and make sure we have a URL to
	 * change the annotation on.
	 */
	ReadArgs(in, out);
	if (ArgUrl == NULL)
	{
		client_error(out, BAD_ANN_REQUEST);
	}
/*
fprintf(out, "<PRE>\n");
fprintf(out, "Got a Delete-Url-Annotations Request\n");
fprintf(out, "URL = %s\nGroup = %s\nUser = %s\nDate = %s\n",
ArgUrl, ArgGroup, ArgUser, ArgDate);
fprintf(out, "Length = %d\n", ArgLength);
*/
	data = NULL;
	/*
	 * Get the data for the new annotation.
	 */
	if (ArgLength > 0)
	{
		data = (char *)malloc(ArgLength * sizeof(char));
		if (data == NULL)
		{
			client_error(out, BAD_ANN_REQUEST);
		}
		ret = fread(data, sizeof(char), ArgLength, in);
		if (ret != ArgLength)
		{
			client_error(out, BAD_ANN_REQUEST);
		}
	}


	/*
	 * Construct the full path to the annotation to
	 * be changed.
	 */
	file = (char *)malloc(MAX_STRING_LEN * sizeof(char));
	if (file == NULL)
	{
		client_error(out, BAD_ANN_REQUEST);
	}
	ptr = strrchr(ArgUrl, '/');
	if (ptr == NULL)
	{
		sprintf(file, "%s/%s", ANN_DIR, ArgUrl);
		if (sscanf(ArgUrl, "%d-%d.html", &hash, &i) !=
			2)
		{
			free(file);
			client_error(out, BAD_ANN_REQUEST);
		}
	}
	else
	{
		sprintf(file, "%s%s", ANN_DIR, ptr);
		ptr++;
		if (sscanf(ptr, "%d-%d.html", &hash, &i) !=
			2)
		{
			free(file);
			client_error(out, BAD_ANN_REQUEST);
		}
	}

	/*
	 * Read and lock the logfile.
	 */
	lptr = GetLogData(hash);
	if (lptr == NULL)
	{
		client_error(out, ANN_SERVER_ERROR);
	}

	url = HashtoUrl(lptr, i);
	if (url == NULL)
	{
		UnlockIt();
		client_error(out, ANN_SERVER_ERROR);
	}

	/*
	 * Remove the old annotation.
	 */
	ret = unlink(file);
	if (ret != 0)
	{
		fprintf(out, "Deletion of %s failed.\n", file);
	}
	else
	{
		fprintf(out, "Deletion of %s success!\n", file);
	}
	free(file);

	/*
	 * Write the new annotation.
	 */
	ret = WriteAnnData(url, hash, i,
		ArgTitle, ArgUser, ArgDate, data, ArgLength, 1);
	free(url);
	if (ret < 0)
	{
		UnlockIt();
		client_error(out, ANN_SERVER_ERROR);
	}

	UnlockIt();

	free(data);
}


/*
 * We know we have received a DELETE_ANN request.  Process it.
 */
void Delete_request(FILE *in, FILE *out)
{
	int i, ret, hash;
	char *data, *ptr;
	LogData *lptr;

	/*
	 * Read the args, and make sure we have a URL to
	 * delete
	 */
	ReadArgs(in, out);
	if (ArgUrl == NULL)
	{
		client_error(out, BAD_ANN_REQUEST);
	}
/*
fprintf(out, "<PRE>\n");
fprintf(out, "Got a Delete-Url-Annotations Request\n");
fprintf(out, "URL = %s\nGroup = %s\nUser = %s\nDate = %s\n",
ArgUrl, ArgGroup, ArgUser, ArgDate);
fprintf(out, "Length = %d\n", ArgLength);
*/

	/*
	 * Construct the full path to the annotation to be deleted.
	 */
	data = (char *)malloc(MAX_STRING_LEN * sizeof(char));
	if (data == NULL)
	{
		client_error(out, BAD_ANN_REQUEST);
	}
	ptr = strrchr(ArgUrl, '/');
	if (ptr == NULL)
	{
		sprintf(data, "%s/%s", ANN_DIR, ArgUrl);
		if (sscanf(ArgUrl, "%d-%d.html", &hash, &i) !=
			2)
		{
			free(data);
			client_error(out, BAD_ANN_REQUEST);
		}
	}
	else
	{
		sprintf(data, "%s%s", ANN_DIR, ptr);
		ptr++;
		if (sscanf(ptr, "%d-%d.html", &hash, &i) !=
			2)
		{
			free(data);
			client_error(out, BAD_ANN_REQUEST);
		}
	}

	/*
	 * Read an lock the log file
	 */
	lptr = GetLogData(hash);
	if (lptr == NULL)
	{
		client_error(out, ANN_SERVER_ERROR);
	}

	/*
	 * Delete this annotation from the logfile.
	 */
	ret = DeleteLogData(lptr, i);
	if (ret != 1)
	{
		UnlockIt();
		client_error(out, ANN_SERVER_ERROR);
	}

	/*
	 * Delete the actual annotation, and any associated
	 * audio annotations.
	 */
	ret = unlink(data);
	if (ret != 0)
	{
		fprintf(out, "Deletion of %s failed.\n", data);
	}
	else
	{
		char afile[MAX_STRING_LEN];

		fprintf(out, "Deletion of %s success!\n", data);
		/*
		 * Remove any associated audio files
		 */
		sprintf(afile, "%s.au", data);
		unlink(afile);
		sprintf(afile, "%s.aiff", data);
		unlink(afile);
	}
	free(data);

	/*
	 * Write the new log, unlock it, and free the data.
	 */
	ret = WriteLogData(lptr);
	if (ret < 0)
	{
		client_error(out, ANN_SERVER_ERROR);
	}
	FreeLogData(lptr);
}

#endif /* ANNOTATIONS */
