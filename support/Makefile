# For gcc
CC= gcc
# For ANSI compilers
#CC= cc

#For Optimization
#CFLAGS= -O2
#For debugging
CFLAGS= -g

RM= /bin/rm -f
#--- You shouldn't have to edit anything else. ---

.c.o: 
	$(CC) -c $(CFLAGS) $<

all: htpasswd unescape

ibm: $(OBJS)
	make all CC=gcc

sun: $(OBJS)
	make all CC=gcc

hp: $(OBJS)
	make all CC=gcc

sgi: $(OBJS)
	make all CC=cc

decmips: $(OBJS)
	make all CC=cc

decaxp: $(OBJS)
	make all CC=cc

tar: htpasswd unescape
	$(RM) htpasswd unescape

htpasswd: htpasswd.c
	$(CC) $(CFLAGS) htpasswd.c -o htpasswd

unescape: unescape.c
	$(CC) $(CFLAGS) unescape.c -o unescape
	
clean:
	rm -f htpasswd

