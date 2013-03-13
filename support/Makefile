## NCSA HTTPd 
# Support directory makefile

# For gcc
CC= gcc
# For ANSI compilers
#CC= cc

RM= /bin/rm -f

#For Optimization
#CFLAGS= -O2
#For debugging
CFLAGS= -g
# For SCO ODT
#EXTRA_LIBS= -lcrypt_i
# For DBM Support
#EXTRA_LIBS = -lndbm 
# For ATTSVR3
#CFLAGS = -DNO_STDLIB_H

# Need src directory for config.h and portability.h.  May need more for DBM
INCLUDES = -I../src

#--------------------------------------------------------------------------
#--------------- You shouldn't have to edit anything else. ----------------
#--------------------------------------------------------------------------

SUPPORT = htpasswd unescape inc2shtml htdigest dbm2std std2dbm dbmdigest \
	dbmgroup dbmpasswd
.c.o: 
	$(CC) -c $(CFLAGS) $(INCLUDES) $<

all: $(SUPPORT)

aix3:
	make all CC=gcc CFLAGS=-DAIX3

aix4:
	make all CC=gcc CFLAGS=-DAIX4

aux:
	make all CC=gcc CFLAGS="-O2 -DAUX -D_POSIX_SOURCE"

hp-cc:
	make all CC=cc CFLAGS="-Ae -DHPUX" EXTRA_LIBS=-lndbm

hp-gcc:
	make all CC=gcc CFLAGS="-DHPUX" EXTRA_LIBS=-lndbm

linux:
	make all CC=gcc CFLAGS="-DLINUX" EXTRA_LIBS=-lgdbm

netbsd: 
	make all CC=cc CFLAGS="-DNETBSD" EXTRA_LIBS=-lcrypt

next:
	make all CC=cc CFLAGS="-DNeXT" EXTRA_LIBS=-ldbm

osf1:
	make all CC=cc CFLAGS="-DOSF1"

sgi4: 
	make all CC=cc CFLAGS="-DIRIX -DHEAD_GETPASS"

sgi5:
	make all CC=cc CFLAGS="-DIRIX"

solaris:
	make all CC=gcc CFLAGS="-DSOLARIS2"

sunos:
	make all CC=gcc CFLAGS="-DSUNOS"

svr4:
	make all CC=cc CFLAGS="-I/usr/include -I/usr/ucbinclude -DSVR4" \
	 EXTRA_LIBS="-lc -L/usr/ucblib -ldbm -lucb"

ultrix:
	make all CC=gcc CFLAGS="-DULTRIX"

htpasswd: htpasswd.c
	$(CC) $(CFLAGS) $(INCLUDES) htpasswd.c -o htpasswd $(EXTRA_LIBS)

dbmpasswd: dbmpasswd.c
	$(CC) $(CFLAGS) $(INCLUDES) dbmpasswd.c -o dbmpasswd $(EXTRA_LIBS)

dbmgroup: dbmgroup.c
	$(CC) $(CFLAGS) $(INCLUDES) dbmgroup.c -o dbmgroup $(EXTRA_LIBS)

std2dbm: std2dbm.c
	$(CC) $(CFLAGS) $(INCLUDES) std2dbm.c -o std2dbm $(EXTRA_LIBS)

dbm2std: dbm2std.c
	$(CC) $(CFLAGS) $(INCLUDES) dbm2std.c -o dbm2std $(EXTRA_LIBS)


DIGESTOBJS=../src/md5.o ../src/md5c.o
../src/md5.o: ../src/md5.h
	$(CC) $(CFLAGS) $(INCLUDES) -c ../src/md5.c -o ../src/md5.o 

../src/md5c.o: ../src/md5.h
	$(CC) $(CFLAGS) $(INCLUDES) -c ../src/md5c.c -o ../src/md5c.o 

htdigest: $(DIGESTOBJS) htdigest.o  ../src/md5.h
	$(CC) $(CFLAGS) -o htdigest htdigest.o $(DIGESTOBJS) $(EXTRA_LIBS)

dbmdigest: $(DIGESTOBJS) dbmdigest.o ../src/md5.h
	$(CC) $(CFLAGS) -o dbmdigest dbmdigest.o $(DIGESTOBJS) $(EXTRA_LIBS)

unescape: unescape.c
	$(CC) $(CFLAGS) $(INCLUDES) unescape.c -o unescape

inc2shtml: inc2shtml.c
	$(CC) $(CFLAGS) $(INCLUDES) inc2shtml.c -o inc2shtml

clean:
	rm -f $(SUPPORT) $(DIGESTOBJS) tags TAGS *.o

