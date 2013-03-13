# Makefile for NCSA's httpd. 

# For normal machines with ANSI compilers
CC= cc
# For Suns or other non-ANSI platforms
#CC= gcc

# For optimization
#CFLAGS= -O2
# For debugging information
#CFLAGS= -g
# If your system does not have strdup(), then do this
# CFLAGS = -O2 -DNEED_STRDUP

# Place here any flags you may need upon linking, such as a flag to
# prevent dynamic linking (if desired)
LFLAGS= 

# Place here any extra libraries you may need to link to. You
# shouldn't have to.
EXTRA_LIBS=

# You shouldn't have to edit anything else.

OBJS=http_config.o httpd.o http_request.o util.o http_dir.o http_gopher.o \
	ann_request.o ann_set.o

.c.o:
	$(CC) -c $(CFLAGS) $(DEFINES) $<

all: httpd

httpd: $(OBJS) 
	$(CC) $(LFLAGS) -o httpd $(OBJS) $(EXTRA_LIBS)

$(OBJS): Makefile httpd.h

clean:
	rm -f httpd $(OBJS)
