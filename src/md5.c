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
 * md5.c,v 1.6 1996/03/06 23:21:21 blong Exp
 *
 ************************************************************************
 *
 * md5.c: NCSA HTTPd code which uses the md5c.c RSA Code
 *
 *  Original Code Copyright (C) 1994, Jeff Hostetler, Spyglass, Inc.
 *  Portions of Content-MD5 code Copyright (C) 1993, 1994 by Carnegie Mellon
 *     University (see Copyright below).
 *  Portions of Content-MD5 code Copyright (C) 1991 Bell Communications 
 *     Research, Inc. (Bellcore) (see Copyright below).
 *  Portions extracted from mpack, John G. Myers - jgm+@cmu.edu
 *  Content-MD5 Code contributed by Martin Hamilton (martin@net.lut.ac.uk)
 *
 */



/* md5.c --Module Interface to MD5. */
/* Jeff Hostetler, Spyglass, Inc., 1994. */


#include "config.h"
#include "portability.h"
#include "constants.h"

#include <stdio.h>
#include <string.h>
#ifndef SHTTP
#include "global.h" 
#endif /* SHTTP */
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


#ifdef CONTENT_MD5
/* these portions extracted from mpack, John G. Myers - jgm+@cmu.edu */

/* (C) Copyright 1993,1994 by Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of Carnegie
 * Mellon University not be used in advertising or publicity
 * pertaining to distribution of the software without specific,
 * written prior permission.  Carnegie Mellon University makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * Copyright (c) 1991 Bell Communications Research, Inc. (Bellcore)
 *
 * Permission to use, copy, modify, and distribute this material
 * for any purpose and without fee is hereby granted, provided
 * that the above copyright notice and this permission notice
 * appear in all copies, and that the name of Bellcore not be
 * used in advertising or publicity pertaining to this
 * material without the specific, prior written permission
 * of an authorized representative of Bellcore.  BELLCORE
 * MAKES NO REPRESENTATIONS ABOUT THE ACCURACY OR SUITABILITY
 * OF THIS MATERIAL FOR ANY PURPOSE.  IT IS PROVIDED "AS IS",
 * WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES.  
 */

static char basis_64[] =
   "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char *md5contextTo64(context)
MD5_CTX *context;
{
    unsigned char digest[18];
    char *encodedDigest;
    int i;
    char *p;

    encodedDigest = (char *)malloc(25 * sizeof(char));

    MD5Final(digest, context);
    digest[sizeof(digest)-1] = digest[sizeof(digest)-2] = 0;

    p = encodedDigest;
    for (i=0; i < sizeof(digest); i+=3) {
        *p++ = basis_64[digest[i]>>2];
        *p++ = basis_64[((digest[i] & 0x3)<<4) | ((digest[i+1] & 0xF0)>>4)];
        *p++ = basis_64[((digest[i+1] & 0xF)<<2) | ((digest[i+2] & 0xC0)>>6)];+         *p++ = basis_64[digest[i+2] & 0x3F];
    }
    *p-- = '\0';
    *p-- = '=';
    *p-- = '=';
    return encodedDigest;
}


char *md5digest(infile)
FILE *infile;
{
    MD5_CTX context;
    char buf[1000];
    long length = 0;
    int nbytes;

    MD5Init(&context);
    while (nbytes = fread(buf, 1, sizeof(buf), infile)) {
        length += nbytes;
        MD5Update(&context, buf, nbytes);
    }
    rewind(infile);
    return md5contextTo64(&context);
}

#endif /* CONTENT_MD5 */


