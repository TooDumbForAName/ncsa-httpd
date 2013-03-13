### NCSA HTTPd 1.5
## Top level Makefile

all:
	@echo Please choose a system type.
	@echo Valid types are aix3, aix4, sunos, sgi4, sgi5,  
	@echo hp-cc, hp-gcc, solaris, netbsd, svr4, linux, 
	@echo next, ultrix, osf1, aux, bsdi
	@echo If you do not have one of these systems, you must edit
	@echo src/Makefile, src/portability.h, src/config.h,
	@echo cgi-src/Makefile, and support/Makefile

clean:
	(cd src ; make clean)
	(cd cgi-src ; make clean)
	(cd support ; make clean)
	rm -f httpd

aix3:
	cd src ; make aix3 ; cd ../cgi-src ; make aix3 ; cd ../support ; make aix3

aix4:
	cd src ; make aix4 ; cd ../cgi-src ; make aix4 ; cd ../support ; make aix4

aux:
	cd src ; make aux ; cd ../cgi-src ; make aux ; cd ../support ; make aux

bsdi:
	cd src ; make ; cd ../cgi-src ; make ; cd ../support ; make

hp-cc:
	cd src ; make hp-cc ; cd ../cgi-src ; make hp-cc ; cd ../support ; make hp-cc

hp-gcc:
	cd src ; make hp-gcc ; cd ../cgi-src ; make hp-gcc ; cd ../support ; make hp-gcc

linux:
	cd src ; make linux ; cd ../cgi-src ; make linux ; cd ../support ; make linux

netbsd:
	cd src ; make netbsd ; cd ../cgi-src ; make netbsd ; cd ../support ; make netbsd

next:
	cd src ; make next ; cd ../cgi-src ; make next; cd ../support ; make next

osf1:
	cd src ; make osf1; cd ../cgi-src ; make osf1; cd ../support ; make osf1 

sgi4:
	cd src ; make sgi; cd ../cgi-src ; make sgi ; cd ../support ; make sgi4

sgi5:
	cd src ; make sgi; cd ../cgi-src ; make sgi ; cd ../support ; make sgi5

solaris:
	cd src ; make solaris ; cd ../cgi-src ; make solaris ; cd ../support ; make solaris 

sunos:
	cd src ; make sunos ; cd ../cgi-src ; make sunos ; cd ../support ; make sunos

svr4:
	cd src ; make svr4 ; cd ../cgi-src ; make svr4 ; cd ../support ; make svr4

ultrix:
	cd src ; make ultrix; cd ../cgi-src ; make ultrix; cd ../support ; make ultrix 
