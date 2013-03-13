# Makefile for NCSA's httpd. 

# For normal machines with ANSI compilers
# CC= cc
# For Suns or other non-ANSI platforms. Please make sure your gcc is
# 2.0 or later, as 1.40 seems to create bad code for the Sun 4.
CC= gcc 

# CFLAGS, compile flags.

# If you want no reverse hostname resolution, use -DMINIMAL_DNS
# If you want to have more secure hostname resolution at the cost of some 
# performance, use -DMAXIMUM_DNS
# -DPEM_AUTH No longer does anything, as the PEM/PGP hooks have been removed
# pursuant to compliance with U.S. State Department and NSA export rules
# If you want to have the server check the execute bit of an HTML file to
# determine if it is a parsed file, use -DXBITHACK
# If you don't want to use the multiple child architecture, use -DNO_PASS
# If you want to ensure that CGI scripts can't mess with the log files,
# use -DSECURE_LOGS
#
CFLAGS= -O2  -g 
#CFLAGS= -g -Wall -ansi -pedantic

# Place here any extra libraries you may need to link to. You
# shouldn't have to.
EXTRA_LIBS=

# AUX_CFLAGS are system-specific control flags.
# NOTE: IF YOU DO NOT CHOOSE ONE OF THESE, EDIT httpd.h AND CHOOSE
# SETTINGS FOR THE SYSTEM FLAGS. IF YOU DON'T, BAD THINGS WILL HAPPEN.

# For SunOS 4:  Only use uncomment EXTRA_LIBS if you want to circumvent
# NIS and /etc/hosts file.
# AUX_CFLAGS= -DSUNOS4
# EXTRA_LIBS= -lresolv
# For Solaris 2. NOTE: Lots of problems have been identified with compiling
# httpd under Solaris. Use with caution. If you see aberrant behavior with
# httpd under Solaris 2, please mail us.
# AUX_CFLAGS= -DSOLARIS2
# EXTRA_LIBS= -lsocket -lnsl
# For SGI IRIX. Use the EXTRA_LIBS line if you're using NIS and want
# user-supported directories
# AUX_CFLAGS= -DIRIX
# EXTRA_LIBS= -lsun
# For HP-UX
# AUX_CFLAGS= -DHPUX
# For AIX3
# AUX_CFLAGS= -DAIX3 -U__STR__
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
# For NetBSD 1.0
# May not need -lcrypt if its included in your libc
# AUX_CFLAGS= -DNETBSD
# EXTRA_LIBS = -lcrypt
# For A/UX
# AUX_CFLAGS= -DAUX -D_POSIX_SOURCE
# EXTRA_LIBS= -lposix -lbsd -s
# For SCO ODT
# libcrypt_i available from sosco.sco.com, files /SLS/lng225b.Z and
# /SLS/lng225b.ltr.Z
# AUX_CFLAGS= -DSCO
# EXTRA_LIBS= -lPW -lsocket -lmalloc # -lcrypt_i
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
 

# Place here any flags you may need upon linking, such as a flag to
# prevent dynamic linking (if desired)
LFLAGS= 

# You shouldn't have to edit anything else.

OBJS=http_config.o httpd.o http_request.o util.o http_dir.o \
http_alias.o http_log.o http_mime.o http_access.o http_auth.o \
http_get.o http_post.o http_script.o http_include.o rfc931.o \
http_put.o http_delete.o http_ipc.o

.c.o:
	$(CC) -c $(CFLAGS) $(AUX_CFLAGS) $<

all: httpd

aux:
	make tar AUX_CFLAGS="-DAUX -D_POSIX_SOURCE" CC=gcc \
		CFLAGS="-O2" EXTRA_LIBS="-lposix -lbsd -s"

aix3:
	make tar AUX_CFLAGS=-DAIX3 CC=gcc CFLAGS=-O2
	
aix4:
	make tar AUX_CFLAGS=-DAIX4 CC=gcc CFLAGS=-O2

sunos:
	make tar AUX_CFLAGS=-DSUNOS4 CC=gcc CFLAGS=-O

solaris:
	make tar AUX_CFLAGS=-DSOLARIS2 EXTRA_LIBS="-lsocket -lnsl" CC=gcc CFLAGS=-O

hp-gcc:
	make tar AUX_CFLAGS=-DHPUX CC=gcc CFLAGS=-O2

hp-cc:
	make tar AUX_CFLAGS=-DHPUX CC=cc CFLAGS="-O -Aa"

sgi:
	make tar AUX_CFLAGS=-DIRIX EXTRA_LIBS=-lsun CC=cc CFLAGS=-O2

decmips:
	make tar AUX_CFLAGS=-DULTRIX CC=cc CFLAGS=-O2

decaxp:
	make tar AUX_CFLAGS=-DOSF1 CC=cc CFLAGS=-O2

netbsd:
	make tar AUX_CFLAGS=-DNETBSD EXTRA_LIBS=-lcrypt CC=cc CFLAGS=-O2

linux:
	make tar AUX_CFLAGS=-DLINUX CC=gcc CFLAGS=-O2

qnx:
	make tar AUX_CFLAGS="-Osax -g1 -w4 -zc -fi=unix.h" EXTRA_LIBS="-N 250k" CC=cc

svr4:
	make tar AUX_CFLAGS=-DSVR4 EXTRA_LIBS="-lsocket -lnsl" CC=cc CFLAGS=-O2

httpd: $(OBJS)
	$(CC) $(LFLAGS) -o httpd $(OBJS) $(EXTRA_LIBS)

purify: $(OBJS)
	purify -logfile=/X11/blong/httpd/logs/pure_log \
	       -program-name=/X11/blong/httpd/src/httpd \
	       -follow-child-processes \
	$(CC) $(LFLAGS) -o httpd $(OBJS) $(EXTRA_LIBS)

tar: $(OBJS)
	$(CC) $(LFLAGS) -o ../httpd $(OBJS) $(EXTRA_LIBS)
	rm -f $(OBJS)

http_ipc.o: Makefile httpd.h new.h
httpd.o: Makefile httpd.h new.h
http_log.o: Makefile httpd.h new.h
http_script.o: Makefile httpd.h new.h

$(OBJS): Makefile httpd.h

clean:
	rm -f httpd $(OBJS) *pure*
