#include "httpd.h"

#ifdef ANNOTATIONS

#define NCSA_GROUP_ANNOTATION_FORMAT_ONE "<ncsa-group-annotation-format-1>"

#define BUFF_SIZE 8192


/*
 * This modulke does all the file locking using flock().
 * Lock is the file descriptor of the lock, when a lock is held, and
 * is -1 when no lock is held.
 */
static int Lock = -1;



/*
 * Attempt to acquire a lock of the passed file descriptor.
 * flock() should block until the lock can be gotten.
 */
static int LockIt(int fd)
{
	int ret;

	ret = flock(fd, LOCK_EX);
	if (ret == 0)
	{
		Lock = fd;
	}
	else
	{
		Lock = -1;
	}
	return(ret);
}


/*
 * If a lock is held, release it.
 */
void UnlockIt(void)
{
	if (Lock != -1)
	{
		flock(Lock, LOCK_UN);
		close(Lock);
		Lock = -1;
	}
}


/*
 * flock() on some systems requires us to not use the buffered I/O.
 * So this is a non-buffered I/O function to read from the file descritor
 * until the character cstop, a newline, or the EOF is encountered.
 */
char *GetUpTo(int fd, char cstop)
{
	char *buf;
	int i, size;

	buf = (char *)malloc(BUFF_SIZE * sizeof(char));
	if (buf == NULL)
	{
		return(NULL);
	}

	for (i=0; i<BUFF_SIZE; i++)
	{
		int ret;
		char val;

		ret = read(fd, &val, 1);
		if (ret != 1)
		{
			return(NULL);
		}
		if (val == cstop)
		{
			buf[i] = '\0';
			break;
		}
		else if (val == '\n')
		{
			buf[i] = '\0';
			break;
		}
		buf[i] = val;
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
			int ret;
			char val;

			ret = read(fd, &val, 1);
			if (ret != 1)
			{
				return(NULL);
			}

			if (val == cstop)
			{
				tptr[i] = '\0';
				break;
			}
			else if (val == '\n')
			{
				tptr[i] = '\0';
				break;
			}
			tptr[i] = val;
		}
		size += i;
	}
	return(buf);
}



	


/*
 * This is the only place locks are acquired.  It is called as part of
 * any annotation action (Set, Delete, Change, Request).  I originally did
 * not have requests for all annotations on a url locking the log file since
 * it was a read only operation, but I didn't want the file changing
 * halfway through a read, so now it locks.
 */
LogData *GetLogData(int hash)
{
	char filename[256];
	char *buf;
	int fd;
	LogData *lptr;
	int i, cnt, acnt, indx, error;

	lptr = (LogData *)malloc(sizeof(LogData));
	if (lptr == NULL)
	{
		return(NULL);
	}

	lptr->hash = hash;
	lptr->cnt = 0;
	lptr->logs = NULL;

	sprintf(filename, "%s/%d.log", ANN_DIR, hash);
	fd = open(filename, O_RDWR);

	/*
	 * No log file.  This is the first annotation on this url.
	 */
	if (fd < 0)
	{
		return(lptr);
	}

	/*
	 * Lock the log file.
	 */
	i = LockIt(fd);
	if (i != 0)
	{
		close(fd);
		return(NULL);
	}

	/*
	 * Read how many urls have hashed to this log file.
	 */
	buf = GetUpTo(fd, '\n');
	if (buf == NULL)
	{
		return(lptr);
	}
	cnt = atoi(buf);
	free(buf);

	if ((cnt < 1)||((lptr->logs =
		(LogRec *)malloc(sizeof(LogRec) * cnt)) == NULL))
	{
		return(lptr);
	}

	error = 0;
	for (i=0; i<cnt; i++)
	{
		int l;
		char *url;

		/*
		 * read the url
		 */
		url = GetUpTo(fd, ' ');

		/*
		 * read the number of annotations on this url
		 */
		buf = GetUpTo(fd, ' ');
		if (buf == NULL)
		{
			error = 1;
			break;
		}
		acnt = atoi(buf);
		free(buf);

		/*
		 * Read the annotation numbers, and fill in the log record.
		 */
		if ((url == NULL)||
		    (*url == '\0')||
		    (acnt <= 0))
		{
			error = 1;
			break;
		}
		lptr->logs[i].url = strdup(url);
		free(url);
		if (lptr->logs[i].url == NULL)
		{
			error = 1;
			break;
		}
		lptr->logs[i].ann_array = (int *)malloc(acnt * sizeof(int));
		if (lptr->logs[i].ann_array == NULL)
		{
			error = 1;
			break;
		}
		lptr->logs[i].acnt = acnt;
		for (l=0; l<acnt; l++)
		{
			buf = GetUpTo(fd, ' ');
			if (buf == NULL)
			{
				error = 1;
				break;
			}
			indx = atoi(buf);
			free(buf);
			lptr->logs[i].ann_array[l] = indx;
		}
		buf = GetUpTo(fd, '\n');
		if ((error)||(buf == NULL))
		{
			break;
		}
	}

	/*
	 * Something failed, free everything up, and return
	 * an empty log.
	 */
	if (error)
	{
		int l;

		for (l=0; l<cnt; l++)
		{
			if (lptr->logs[l].url != NULL)
			{
				free(lptr->logs[l].url);
			}
			if (lptr->logs[l].ann_array != NULL)
			{
				free(lptr->logs[l].ann_array);
			}
		}
		free(lptr->logs);
		lptr->logs = NULL;
		return(lptr);
	}

	lptr->cnt = cnt;
	return(lptr);
}


/*
 * Look through the logdata, and find all the annotations on
 * a given url.
 */
int FindLogData(LogData *lptr, char *url, int **alist, int *acnt)
{
	int i, indx, cnt;
	int *aptr;

	*acnt = 0;
	*alist = NULL;
	if ((lptr == NULL)||(lptr->cnt == 0))
	{
		return(-1);
	}

	/*
	 * Find the url in the log data.
	 */
	cnt = 0;
	for (i=0; i < lptr->cnt; i++)
	{
		if ((lptr->logs[i].url != NULL)&&
			(strcmp(lptr->logs[i].url, url) == 0))
		{
			break;
		}
	}

	/*
	 * The url is not in this log
	 */
	if (i == lptr->cnt)
	{
		return(0);
	}

	indx = i;

	/*
	 * Fill in the array of annotations, and return.
	 */
	cnt = lptr->logs[indx].acnt;
	if (cnt == 0)
	{
		return(0);
	}
	aptr = (int *)malloc(cnt * sizeof(int));
	if (aptr == NULL)
	{
		return(-1);
	}
	for (i=0; i < cnt; i++)
	{
		aptr[i] = lptr->logs[indx].ann_array[i];
	}
	*alist = aptr;
	*acnt = cnt;
	return(cnt);
}


/*
 * Add a new annotation to a url in a given logdata
 */
int AddLogData(LogData *lptr, int indx, char *url)
{
	char **tarray;
	int i, cnt;
	LogRec *lrec;

	if (lptr == NULL)
	{
		return(0);
	}

	/*
	 * Find the url in the logdata
	 */
	lrec = NULL;
	for (i=0; i < lptr->cnt; i++)
	{
		if ((lptr->logs[i].url != NULL)&&
			(strcmp(lptr->logs[i].url, url) == 0))
		{
			lrec = &(lptr->logs[i]);
			break;
		}
	}

	/*
	 * This url does not currently have an entry in this logdata.
	 * Make a new entry for it.  Allocate space for all the current 
	 * urls, plus one.  Copy all the old data forward, fill in the
	 * new data, then free up the old space.
	 */
	if (lrec == NULL)
	{
		if ((lrec = (LogRec *)malloc((lptr->cnt + 1) * sizeof(LogRec)))
			== NULL)
		{
			return(0);
		}
		lrec[lptr->cnt].url = strdup(url);
		if (lrec[lptr->cnt].url == NULL)
		{
			return(0);
		}
		if ((lrec[lptr->cnt].ann_array = (int *)malloc(sizeof(int)))
			== NULL)
		{
			return(0);
		}
		bcopy(lptr->logs, lrec, (lptr->cnt * sizeof(LogRec)));
		free((char *)lptr->logs);
		lptr->logs = lrec;
		lrec = &(lptr->logs[lptr->cnt]);
		lptr->cnt++;
		lrec->acnt = 1;
		lrec->ann_array[0] = indx;
	}
	/*
	 * Add this annotation the the ones already on this url.
	 */
	else
	{
		int *iptr;

		/*
		 * If this annotation is already recorded on this url, do
		 * nothing and return.
		 */
		for (i=0; i < lrec->acnt; i++)
		{
			if (lrec->ann_array[i] == indx)
			{
				return(0);
			}
		}

		/*
		 * Make room for the new annotation.  Copy the old
		 * annotations forward, and fill in the new one.
		 * free the old space.
		 */
		iptr = (int *)malloc((lrec->acnt + 1) * sizeof(int));
		if (iptr == NULL)
		{
			return(0);
		}
		bcopy(lrec->ann_array, iptr, (lrec->acnt * sizeof(int)));
		free((char *)lrec->ann_array);
		lrec->ann_array = iptr;
		lrec->ann_array[lrec->acnt] = indx;
		lrec->acnt++;
	}

	return(1);
}


/*
 * Give a log data structure and the index, find the url
 * of the document this annotation resides on.
 */
char *HashtoUrl(LogData *lptr, int indx)
{
	char *url;
	int i, j;

	if (lptr == NULL)
	{
		return(0);
	}

	for (i=0; i < lptr->cnt; i++)
	{
		for (j=0; j < lptr->logs[i].acnt; j++)
		{
			if (lptr->logs[i].ann_array[j] == indx)
			{
				url = strdup(lptr->logs[i].url);
				return(url);
			}
		}
	}
	return(NULL);
}


/*
 * Write out the log data to a logfile.
 */
int WriteLogData(LogData *lptr)
{
	char filename[256];
	char buf[BUFF_SIZE];
	int fd;
	int i, j;

	if (lptr == NULL)
	{
		return(-1);
	}

	sprintf(filename, "%s/%d.log", ANN_DIR, lptr->hash);

	/*
	 * If there are no more annotations tolog here, remove the
	 * file, and unlock it (if locked).
	 */
	if (lptr->cnt == 0)
	{
		unlink(filename);
		UnlockIt();
		return(0);
	}

	/*
	 * Move to the beginning of a locked log file, or create a
	 * new one if this is the first annotation.
	 */
	if (Lock != -1)
	{
		fd = Lock;
		lseek(fd, 0, 0);
	}
	else
	{
		fd = open(filename, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	}
	if (fd < 0)
	{
		return(-1);
	}

	/*
	 * Write out the log file.  We can't use buffered I/O here
	 * because the file is locked with flock().
	 */
	sprintf(buf, "%d\n", lptr->cnt);
	write(fd, buf, strlen(buf));
	for (i=0; i < lptr->cnt; i++)
	{
		sprintf(buf, "%s %d ", lptr->logs[i].url, lptr->logs[i].acnt);
		write(fd, buf, strlen(buf));
		for (j=0; j < lptr->logs[i].acnt; j++)
		{
			sprintf(buf, "%d ", lptr->logs[i].ann_array[j]);
			write(fd, buf, strlen(buf));
		}
		sprintf(buf, "\n");
		write(fd, buf, strlen(buf));
	}

	/*
	 * Truncate the file in case is has become shorter, then unlock
	 * and close it.
	 */
	ftruncate(fd, tell(fd));
	UnlockIt();
	return(1);
}


/*
 * Free up all the memory allocated to store a read in logfile.
*/
void FreeLogData(LogData *lptr)
{
	int l;

	for (l=0; l < lptr->cnt; l++)
	{
		if (lptr->logs[l].url != NULL)
		{
			free(lptr->logs[l].url);
		}
		if (lptr->logs[l].ann_array != NULL)
		{
			free((char *)(lptr->logs[l].ann_array));
		}
	}
	free((char *)(lptr->logs));
	free((char *)lptr);
}


/*
 * Write a new audio annotation with the passed index.  If no_change is
 * not set, we are allowed to change the index to find a filename that
 * isn't already in use.
 * On success return the url of the new annotation, on failure
 * return NULL.
 */
char *WriteAudioData(int hash, int indx, char *data, int len, char *type, int no_change)
{
	char filename[256];
	char *url;
	FILE *fp;
	int orig_i;
	char *host;

        host = full_hostname ();

	/*
	 * Find the next unused filename in a sequence
	 * starting with indx.
	 */
	orig_i = indx;
	sprintf(filename, "%s/%d-%d.html", ANN_DIR, hash, indx);
	fp = fopen(filename, "r");
	while (fp != NULL)
	{
		fclose(fp);
		indx++;
		sprintf(filename, "%s/%d-%d.html", ANN_DIR, hash, indx);
		fp = fopen(filename, "r");
	}

	if ((orig_i != indx)&&(no_change))
	{
		return(NULL);
	}

	/*
	 * Write the sound data.  Currently either .au or .aiff
	 */
	sprintf(filename, "%s/%d-%d.html.%s", ANN_DIR, hash, indx, type);
	fp = fopen(filename, "w");
	if (fp == NULL)
	{
		return(NULL);
	}
	fwrite(data, sizeof(char), len, fp);
	fclose(fp);

	/*
	 * Construct and return the url for this annotation.
	 */
	url = (char *)malloc(strlen(filename) + 512);
	if (url == NULL)
	{
		return(NULL);
	}
	sprintf(url, "http://%s:%d%s/%d-%d.html.%s",
				host, ANN_PORT, ANN_VIRTUAL_DIR, hash, indx, type);
	return(url);
}


/*
 * Write a new annotation with the passed index.  If no_change is
 * not set, we are allowed to change the index to find a filename that
 * isn't already in use.
 * On success return the index of the annotation actually written, on
 * failure return -1.
 */
int WriteAnnData(char *url, int hash, int indx, char *title, char *user, char *date, char *data, int len, int no_change)
{
	char filename[256];
	FILE *fp;
	int orig_i;

	/*
	 * Find the next unused filename in a sequence
	 * starting with indx.
	 */
	orig_i = indx;
	sprintf(filename, "%s/%d-%d.html", ANN_DIR, hash, indx);
	fp = fopen(filename, "r");
	while (fp != NULL)
	{
		fclose(fp);
		indx++;
		sprintf(filename, "%s/%d-%d.html", ANN_DIR, hash, indx);
		fp = fopen(filename, "r");
	}

	if ((orig_i != indx)&&(no_change))
	{
		return(-1);
	}

	/*
	 * Write the annotation.
	 */
	fp = fopen(filename, "w");
	if (fp == NULL)
	{
		return(-1);
	}
	fprintf(fp, "%s\n", NCSA_GROUP_ANNOTATION_FORMAT_ONE);
	fprintf(fp, "<title>%s</title>\n", title);
	fprintf(fp, "<h1>%s</h1>\n", title);
	fprintf(fp, "<address>%s</address>\n", user);
	fprintf(fp, "<address>%s</address>\n", date);
	fprintf(fp, "<address>(<a HREF=\"%s\">Back</a> to annotated document)</address>", url);
	fprintf(fp, "______________________________________\n");
	fprintf(fp, "<pre>\n");
	fwrite(data, sizeof(char), len, fp);
	fclose(fp);
	return(indx);
}


/*
 * read the requested annotation, extracting title, user, and date.
 */
int ReadAnnData(int hash, int indx, char **title, char **user, char **date)
{
	char filename[256];
	char *buf;
	FILE *fp;
	int val;
	int size, cnt;

	sprintf(filename, "%s/%d-%d.html", ANN_DIR, hash, indx);
	fp = fopen(filename, "r");
	if (fp == NULL)
	{
		return(0);
	}

	/*
	 * Skip the first line, containing the magic cookie for mosaic
	 */
	val = fgetc(fp);
	while (val != '\n')
	{
		if (val == EOF)
		{
			return(0);
		}
		val = fgetc(fp);
	}

	/*
	 * Skip this line, it is the <TITLE> mark.
	 */
	val = fgetc(fp);
	while (val != '\n')
	{
		if (val == EOF)
		{
			return(0);
		}
		val = fgetc(fp);
	}

	/*
	 * Skip past the <h1>
	 */
	val = fgetc(fp);
	while (val != '>')
	{
		if (val == EOF)
		{
			return(0);
		}
		val = fgetc(fp);
	}

	/*
	 * Read the title
	 */
	buf = (char *)malloc(MAX_STRING_LEN * sizeof(char));
	if (buf == NULL)
	{
		return(0);
	}
	size = MAX_STRING_LEN;
	cnt = 0;
	val = fgetc(fp);
	while (val != '<')
	{
		if (val == EOF)
		{
			return(0);
		}
		buf[cnt] = val;
		val = fgetc(fp);
		cnt++;
		if (cnt == size)
		{
			char *tptr;

			tptr = (char *)malloc((size + MAX_STRING_LEN) *
				sizeof(char));
			if (tptr == NULL)
			{
				return(0);
			}
			bcopy(buf, tptr, size);
			free(buf);
			buf = tptr;
			size += MAX_STRING_LEN;
		}
	}
	buf[cnt] = 0;
	*title = strdup(buf);
	if (*title == NULL)
	{
		return(0);
	}

	/*
	 * Skip the junk at the end of the title line
	 */
	val = fgetc(fp);
	while (val != '\n')
	{
		if (val == EOF)
		{
			return(0);
		}
		val = fgetc(fp);
	}

	/*
	 * Skip the <address>
	 */
	val = fgetc(fp);
	while (val != '>')
	{
		if (val == EOF)
		{
			return(0);
		}
		val = fgetc(fp);
	}

	/*
	 * Read the user info
	 */
	buf = (char *)malloc(MAX_STRING_LEN * sizeof(char));
	if (buf == NULL)
	{
		return(0);
	}
	size = MAX_STRING_LEN;
	cnt = 0;
	val = fgetc(fp);
	while (val != '<')
	{
		if (val == EOF)
		{
			return(0);
		}
		buf[cnt] = val;
		val = fgetc(fp);
		cnt++;
		if (cnt == size)
		{
			char *tptr;

			tptr = (char *)malloc((size + MAX_STRING_LEN) *
				sizeof(char));
			if (tptr == NULL)
			{
				return(0);
			}
			bcopy(buf, tptr, size);
			free(buf);
			buf = tptr;
			size += MAX_STRING_LEN;
		}
	}
	buf[cnt] = 0;
	*user = strdup(buf);
	if (*user == NULL)
	{
		return(0);
	}

	/*
	 * skip the junk at the end of the user line.
	 */
	val = fgetc(fp);
	while (val != '\n')
	{
		if (val == EOF)
		{
			return(0);
		}
		val = fgetc(fp);
	}

	/*
	 * skip the <address>
	 */
	val = fgetc(fp);
	while (val != '>')
	{
		if (val == EOF)
		{
			return(0);
		}
		val = fgetc(fp);
	}

	/*
	 * read the date
	 */
	cnt = 0;
	val = fgetc(fp);
	while (val != '<')
	{
		if (val == EOF)
		{
			return(0);
		}
		buf[cnt] = val;
		val = fgetc(fp);
		cnt++;
		if (cnt == size)
		{
			char *tptr;

			tptr = (char *)malloc((size + MAX_STRING_LEN) *
				sizeof(char));
			if (tptr == NULL)
			{
				return(0);
			}
			bcopy(buf, tptr, size);
			free(buf);
			buf = tptr;
			size += MAX_STRING_LEN;
		}
	}
	buf[cnt] = 0;
	*date = strdup(buf);
	if (*date == NULL)
	{
		return(0);
	}

	fclose(fp);
	return(1);
}

/*
 * find the largest annotation index for this log file.
 */
int MaxIndex(LogData *lptr)
{
	int i, j, indx;

	indx = 0;

	if ((lptr == NULL)||(lptr->cnt == 0))
	{
		return(indx);
	}

	for (i=0; i < lptr->cnt; i++)
	{
		for (j=0; j < lptr->logs[i].acnt; j++)
		{
			if (lptr->logs[i].ann_array[j] > indx)
			{
				indx = lptr->logs[i].ann_array[j];
			}
		}
	}

	/*
	 * WARNING, ARBITRARY LIMIT!  2^14 is the max index
	 */
	if (indx > (0x01 << 14))
	{
		indx = 0;
	}
	return(indx);
}

/*
 * Delete the passed index from the log data.
 */
int DeleteLogData(LogData *lptr, int indx)
{
	int i, j, found;

	if ((lptr == NULL)||(lptr->cnt == 0))
	{
		return(0);
	}

	found = 0;
	for (i=0; i < lptr->cnt; i++)
	{
		for (j=0; j < lptr->logs[i].acnt; j++)
		{
			if (lptr->logs[i].ann_array[j] == indx)
			{
				found = 1;
				break;
			}
		}
		if (found)
		{
			break;
		}
	}
	if (!found)
	{
		return(0);
	}

	if (lptr->logs[i].acnt > 1)
	{
		lptr->logs[i].ann_array[j] =
			lptr->logs[i].ann_array[lptr->logs[i].acnt - 1];
		lptr->logs[i].acnt--;
		return(1);
	}
	else
	{
		if (lptr->logs[i].ann_array != NULL)
		{
			free((char *)lptr->logs[i].ann_array);
		}
		if (lptr->logs[i].url != NULL)
		{
			free((char *)lptr->logs[i].url);
		}

		if (lptr->cnt > 1)
		{
			if (i == (lptr->cnt - 1))
			{
				lptr->cnt--;
				return(1);
			}
			else
			{
				bcopy((char *)&(lptr->logs[lptr->cnt - 1]),
					(char *)&(lptr->logs[i]),
					sizeof(LogRec));
				lptr->cnt--;
				return(1);
			}
		}
		else
		{
			free((char *)lptr->logs);
			lptr->logs = NULL;
			lptr->cnt = 0;
			return(1);
		}
	}
}


#endif /* ANNOTATIONS */
