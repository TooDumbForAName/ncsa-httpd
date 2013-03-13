
all:
	@echo Please choose a system type.
	@echo Valid types are ibm, sunos, sgi, decmips, decaxp, hp
	@echo solaris
	@echo If you do not have one of these systems, you must edit
	@echo src/Makefile, cgi-src/Makefile, and support/Makefile

clean:
	(cd src ; make clean)
	(cd cgi-src ; make clean)
	(cd support ; make clean)

tar-clean: clean
	rm -f httpd

ibm:
	cd src ; make ibm ; cd ../cgi-src ; make ibm ; cd ../support ; make ibm

sunos:
	cd src ; make sunos ; cd ../cgi-src ; make sunos ; cd ../support ; make sunos

solaris:
	cd src ; make solaris ; cd ../cgi-src ; make solaris ; cd ../support ; make solaris 

sgi:
	cd src ; make sgi ; cd ../cgi-src ; make sgi ; cd ../support ; make sgi

decmips:
	cd src ; make decmips ; cd ../cgi-src ; make decmips ; cd ../support ; make decmips

decaxp:
	cd src ; make decaxp ; cd ../cgi-src ; make decaxp ; cd ../support ; make decaxp

hp:
	cd src ; make hp ; cd ../cgi-src ; make hp ; cd ../support ; make hp
