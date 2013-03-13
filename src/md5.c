/* md5.c --Module Interface to MD5. */
/* Jeff Hostetler, Spyglass, Inc., 1994. */


#include "config.h"
#include "portability.h"

#include <stdio.h>
#include <string.h>
#include "global.h"
#include "md5.h"

void md5 (unsigned char *string, char result[33])
{
    MD5_CTX my_md5;
    unsigned char hash[16];
    char *p;
    int i;
	
    /*
     * Take the MD5 hash of the string argument.
     */

    MD5Init(&my_md5);
    MD5Update(&my_md5, string, strlen((const char *)string));
    MD5Final(hash, &my_md5);

    for (i=0, p=result; i<16; i++, p+=2)
        sprintf(p, "%02x", hash[i]);
    *p = '\0';

	return;
}
