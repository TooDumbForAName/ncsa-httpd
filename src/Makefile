# NCSA HTTPd 1.5
#
# For normal machines with ANSI compilers
#CC= cc
# For Suns or other non-ANSI platforms. 
CC= gcc 
#CC = xlc
QUANTIFY=/hdf2/pure/quantify-2.0.1-sunos4/quantify
PURIFY=/hdf2/pure/purify-3.2-sunos4/purify
#PURIFY=/X11/sgi5/purify3.1/purify

# CFLAGS, compile flags.
#
# Note: As of version 1.5, only security and dbm defines are in the Makefile
# All other defines are in config.h
# If you want to ensure that CGI scripts can't mess with the log files,
# use -DSECURE_LOGS

CFLAGS= -g
#CFLAGS= -pg -DPROFILE
#CFLAGS= -g -ansi -pedantic -Wall -DAIX_BROKEN_HEADERS

# DIGEST AUTHENTICATION 
#
# To enable Message Digest Authentication, define the DIGEST_AUTH flag

MD5_CFLAGS = -DDIGEST_AUTH

# KERBEROS 
#
# If you want httpd to support Kerberos authentication, set the
# following flags appropriately.  You can support Kerberos V4 and/or V5,
# although it's most likely that your realm supports one or the other.
# To enable DES-encryption of HTTP messages via Kerberos key exchange,
# define the KRB-ENCRYPT flag (not supported in Beta1 -- hopefully Beta2).

KRB4_DIR    = /xdev/mosaic/libkrb4/sun
KRB4_LIBS   = -L$(KRB4_DIR)/lib -lkrb -ldes
KRB4_CFLAGS = -DKRB4 -I$(KRB4_DIR)/include

KRB5_DIR    = /xdev/mosaic/libkrb5/sun
KRB5_LIBS   = $(KRB5_DIR)/lib/libkrb5.a $(KRB5_DIR)/lib/libcrypto.a $(KRB5_DIR)/util/et/libcom_err.a
KRB5_CFLAGS = -DKRB5 -I$(KRB5_DIR)/include -I$(KRB5_DIR)/include/krb5


# Comment out the following two lines to exclude Kerberos support

#KRB_CFLAGS = $(KRB4_CFLAGS) $(KRB5_CFLAGS)   # -DKRB-ENCRYPT
#KRB_LIBS   = $(KRB4_LIBS) $(KRB5_LIBS)

# DBM 
#
# To enable DBM password/groupfile support, define the DBM_SUPPORT flag

DBM_CFLAGS = -DDBM_SUPPORT
#DBM_LIBS = -lndbm
#DBM_LIBS = -ldbm 
#DBM_LIBS = -lgdbm
# For SVR4
#DBM_CFLAGS = -I/usr/include -I/usr/ucbinclude
#DBM_LIBS = -L/usr/ucblib -ldbm -lucb

# Place here any extra libraries you may need to link to. You
# shouldn't have to.

EXTRA_LIBS=

# Place here any flags you may need upon linking, such as a flag to
# prevent dynamic linking (if desired)
LFLAGS=  # -pg -DPROFILE

# AUX_CFLAGS are system-specific control flags.
# NOTE: IF YOU DO NOT CHOOSE ONE OF THESE, EDIT portability.h AND CHOOSE
# SETTINGS FOR THE SYSTEM FLAGS. IF YOU DON'T, BAD THINGS WILL HAPPEN.

# For SunOS 4:  Only use uncomment EXTRA_LIBS if you want to circumvent
# NIS and /etc/hosts file.
# AUX_CFLAGS= -DSUNOS4
# EXTRA_LIBS= -lresolv
# For Solaris 2.x
# AUX_CFLAGS= -DSOLARIS2
# EXTRA_LIBS= -lsocket -lnsl 
# For SGI IRIX. Use the EXTRA_LIBS line if you're using NIS and want
# user-supported directories
# AUX_CFLAGS= -DIRIX
# EXTRA_LIBS= -lsun
# For HP-UX
# AUX_CFLAGS= -DHPUX
# For AIX3
# AUX_CFLAGS= -DAIX3 -D_ALL_SOURCE # For xlc compiler
# For AIX4
# AUX_CFLAGS= -DAIX4
# For Ultrix
# AUX_CFLAGS= -DULTRIX
# For DEC OSF/1
# AUX_CFLAGS= -DOSF1
# For NeXT
# AUX_CFLAGS= -DNeXT
# For Sequent
# AUX_CFLAGS= -DSEQUENT
# For Linux -m486 ONLY IF YOU HAVE 486 BINARY SUPPORT IN KERNEL
# AUX_CFLAGS= -DLINUX
# DBM_LIBS = -lgdbm
# For NetBSD 1.0
# May not need -lcrypt if its included in your libc
# AUX_CFLAGS= -DNetBSD
# EXTRA_LIBS = -lcrypt
# For FreeBSD 2.0.5
# AUX_CFLAGS= -DFreeBSD
# For A/UX
# AUX_CFLAGS= -DAUX -D_POSIX_SOURCE
# EXTRA_LIBS= -lbsd -lposix -s
# DBM_LIBS= -ldbm
# For SCO ODT
# libcrypt_i available from sosco.sco.com, files /SLS/lng225b.Z and
# /SLS/lng225b.ltr.Z
# AUX_CFLAGS= -DSCO
# EXTRA_LIBS= -lPW -lsocket -lmalloc -lcrypt_i
# For SCO SVR3.2
# AUX_CFLAGS= -DSCO3
# EXTRA_LIBS= -lPW -lsocket -lmalloc -lintl -lcrypt
# For SVR4
# AUX_CFLAGS= -DSVR4
# EXTRA_LIBS= -lsocket -lnsl -lc
# For Amdahl UTS 2.1
# -Xa enables ANSI mode, -eft is expanded types
# AUX_CFLAGS= -Xa -eft -DUTS21
# EXTRA_LIBS= -lsocket -lbsd -la
# For HP/Apollo Domain/OS
# AUX_CFLAGS= -DAPOLLO
# For AT&T Sys V R 3.2
# AUX_CFLAGS= -DATTSVR3
# EXTRA_LIBS= -lnet -lnsl_s -lgen
# For QNX 4.22
# AUX_CFLAGS= -Osax  -g1 -fi=unix.h -w4 -zc -5 
# EXTRA_LIBS= -N 250k
# For ConvexOS 10.1 with Convex C Compiler 5.0
# AUX_CFLAGS= -DCONVEXOS -D__STDC__

# -------------------------------------------------------------------------
# -------------- You shouldn't have to edit anything else -----------------
# -------------------------------------------------------------------------

SEC_CFLAGS = $(MD5_CFLAGS) $(KRB_CFLAGS) 
SEC_LIBS   = $(KRB_LIBS) 

ALL_CFLAGS = $(CFLAGS) $(AUX_CFLAGS) $(SEC_CFLAGS) $(DBM_CFLAGS)
ALL_LIBS = $(EXTRA_LIBS) $(SEC_LIBS) $(DBM_LIBS) 


OBJS=httpd.o http_config.o http_request.o util.o http_dir.o \
http_alias.o http_log.o http_mime.o http_access.o http_auth.o \
http_send.o cgi.o http_include.o rfc931.o imagemap.o \
http_ipc.o digest.o md5.o md5c.o env.o host_config.o fdwrap.o \
open_logfile.o allocate.o debug.o blackout.o 

.c.o: Makefile config.h portability.h constants.h
	$(CC) -c $(ALL_CFLAGS) $<

all: httpd

aix3:
	make tar AUX_CFLAGS=-DAIX3 CC=gcc CFLAGS=-O2

aix4:
	make tar AUX_CFLAGS=-DAIX4 CC=gcc

aux:
	make tar AUX_CFLAGS="-DAUX -D_POSIX_SOURCE" CC="gcc" \
	CFLAGS="-O2" EXTRA_LIBS="-lposix -lbsd -s" DBM_LIBS="-ldbm"

hp-gcc:
	make tar AUX_CFLAGS=-DHPUX CC=gcc CFLAGS=-O2 DBM_LIBS=-lndbm

hp-cc:
	make tar AUX_CFLAGS=-DHPUX CC=cc CFLAGS="-O -Aa" DBM_LIBS=-lndbm

linux:
	make tar AUX_CFLAGS=-DLINUX CC=gcc CFLAGS=-O2 DBM_LIBS=-lgdbm

netbsd:
	make tar AUX_CFLAGS=-DNETBSD EXTRA_LIBS=-lcrypt CC=cc CFLAGS=-O2

next:
	make tar AUX_CFLAGS=-DNeXT CC=cc CFLAGS=-O2 DBM_LIBS=-ldbm

osf1:
	make tar AUX_CFLAGS=-DOSF1 CC=cc CFLAGS="-O2 -Olimit 750"

sgi:
	make tar AUX_CFLAGS=-DIRIX EXTRA_LIBS=-lsun CC=cc CFLAGS=-O2

solaris:
	make tar AUX_CFLAGS=-DSOLARIS2 EXTRA_LIBS="-lsocket -lnsl" CC=gcc CFLAGS=-O

sunos:
	make tar AUX_CFLAGS=-DSUNOS4 CC=gcc

svr4:
	make tar AUX_CFLAGS=-DSVR4 EXTRA_LIBS="-lsocket -lnsl -lc" CC=cc \
	 CFLAGS=-O2 DBM_LIBS="-L/usr/ucblib -ldbm -lucb" \
	 DBM_CFLAGS="-I/usr/include -I/usr/ucbinclude"

ultrix:
	make tar AUX_CFLAGS=-DULTRIX CC=gcc CFLAGS=-O2

httpd: $(OBJS)
	$(CC) $(LFLAGS) -o httpd $(OBJS) $(ALL_LIBS)

#	-logfile=/X11/blong/httpd/logs/pure_log 
purify: $(OBJS)
	$(PURIFY) -program-name=/X11/blong/httpd/src/httpd \
	          -follow-child-processes=yes \
	$(CC) $(LFLAGS) -o httpd $(OBJS) $(ALL_LIBS) 

quantify: $(OBJS)
	$(QUANTIFY) -record-child-process-data=yes \
		    -record-data=no \
	            -avoid-recording-system-calls=1,6,93,113,114 \
	$(CC) $(LFLAGS) -o httpd $(OBJS) $(ALL_LIBS) 

tar: $(OBJS)
	$(CC) $(LFLAGS) -o ../httpd $(OBJS) $(ALL_LIBS) 


http_access.o:  Makefile config.h portability.h constants.h http_access.h \
		http_request.h http_config.h http_auth.h http_mime.h \
		util.h 
http_alias.o:   config.h portability.h constants.h http_alias.h \
		http_config.h util.h http_log.h
http_auth.o:    Makefile config.h portability.h constants.h http_auth.h \
		http_mime.h http_access.h http_config.h digest.h http_log.h \
		util.h
http_config.o:  Makefile config.h portability.h constants.h http_config.h \
		http_mime.h http_access.h host_config.h http_alias.h \
		http_log.h http_dir.h util.h
host_config.o:  Makefile config.h portability.h constants.h host_config.h \
		http_log.h util.h
http_dir.o:     config.h portability.h constants.h http_dir.h \
		http_mime.h http_log.h http_config.h http_request.h \
		http_alias.h util.h
http_send.o:    config.h portability.h constants.h http_send.h \
		cgi.h http_mime.h http_log.h http_request.h \
		http_config.h http_include.h http_alias.h http_access.h util.h
http_include.o: config.h portability.h constants.h http_include.h \
		util.h http_mime.h http_log.h http_config.h http_request.h \
		cgi.h http_access.h http_alias.h http_dir.h env.h
http_ipc.o:     config.h portability.h constants.h http_ipc.h 
http_log.o:     config.h portability.h constants.h http_log.h \
		http_request.h http_config.h http_auth.h http_mime.h util.h
http_mime.o:    Makefile config.h portability.h constants.h http_mime.h \
		http_log.h util.h http_config.h http_access.h env.h
http_request.o: Makefile config.h portability.h constants.h http_request.h \
		allocate.h cgi.h env.h http_access.h http_alias.h \
		host_config.h http_config.h http_log.h http_send.h util.h
imagemap.o:     Makefile constants.h imagemap.h allocate.h
cgi.o:		Makefile config.h portability.h constants.h cgi.h \
		http_log.h http_request.h util.h http_mime.h http_access.h \
		http_auth.h http_alias.h http_config.h
httpd.o:        Makefile config.h portability.h constants.h httpd.h \
		http_request.h http_config.h http_log.h http_auth.h \
		http_dir.h http_access.h util.h http_ipc.h host_config.h \
		http_mime.h cgi.h
util.o:         config.h portability.h constants.h util.h \
		http_request.h http_config.h host_config.h http_log.h
env.o:		config.h portability.h constants.h http_log.h env.h
rfc931.o:	config.h portability.h 
open_logfile.o: config.h portability.h constants.h open_logfile.h \
		http_config.h util.h
allocate.o:	config.h portability.h constants.h host_config.h http_log.h \
		allocate.h

# file descriptor scoreboarding
fdwrap.o:	config.h portability.h constants.h fdwrap.h

# for digest authentication
digest.o:	Makefile config.h portability.h constants.h digest.h \
		http_request.h http_log.h http_auth.h http_mime.h util.h
md5.o:		Makefile config.h portability.h global.h md5.h
md5c.o:		Makefile config.h portability.h global.h md5.h

clean:
	rm -f httpd $(OBJS) *pure* TAGS tags

SRC = $(OBJS:%.o=%.c)
codecenter:
	#setopt ansi
	#setopt print_string 128
	#setopt load_flags $(ALL_FLAGS) 
	#load $(LFLAGS) $(SRC) $(ALL_LIBS) 
