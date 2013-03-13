#!/usr/local/bin/perl
#
# Non-parsed headers CGI 1.1 error script in Perl to handle error requests
# from NCSA HTTPd 1.4 via ErrorDocument.  This should handle all errors in 
# almost the same fashion as NCSA HTTPd 1.4 would internally.
#
# This script is in the Public Domain.  NCSA and the author offer no
# guaruntee's nor claim any responsibility for it.  That's as pseudo-legalise
# as I get.
#
# This script doesn't do any encryption or authentication, nor does it
# contain hooks to do so.
#
# This was written for Perl 4.016.  I've heard rumours about it working with
# other versions, but I'm no Perl hacker, so how would I know?
#
# Brandon Long / NCSA HTTPd Development Team / Software Development Group 
# National Center for Supercomputing Applications / University of Illinios
#
# For more information:
# NCSA HTTPd    : http://hoohoo.ncsa.uiuc.edu/docs/
# CGI 1.1       : http://hoohoo.nsca.uiuc.edu/cgi/
# ErrorDocument : http://hoohoo.ncsa.uiuc.edu/docs/setup/srm/ErrorDocument.html
# Example CGI   : http://hoohoo.ncsa.uiuc.edu/cgi/ErrorCGI.html
#

$error = $ENV{'QUERY_STRING'};
$redirect_request = $ENV{'REDIRECT_REQUEST'};
($redirect_method,$request_url,$redirect_protocal) = split(' ',$redirect_request);
$redirect_status = $ENV{'REDIRECT_STATUS'};
if (!defined($redirect_status)) {
  $redirect_status = "200 Ok";
}
($redirect_number,$redirect_message) = split(' ',$redirect_status);
$error =~ s/error=//;

$title = "<HEAD><TITLE>".$redirect_status."</TITLE></HEAD>";

if ($redirect_method eq "HEAD") {
	$head_only = 1;
} else {
	$head_only = 0;
}

printf("%s %s\r\n",$ENV{'SERVER_PROTOCOL'},$redirect_status);
printf("Server: %s\r\n",$ENV{'SERVER_SOFTWARE'});
printf("Content-type: text/html\r\n");

$redirect_status = "<img alt=\"\" src=/images/icon.gif>".$redirect_status;
if ($redirect_number == 302) {
	if ($error !~ /http:/) {	
		printf("xLocation: http://%s:%s%s\r\n",
			$ENV{'SERVER_NAME'},
			$ENV{'SERVER_PORT'},
			$error);
	if (!$head_only) {
		printf("%s\n\r",$title);
		printf("<BODY><H1>%s</H1>\n\r",$redirect_status);
		printf("This document has moved");
		printf("<A HREF=\"http://%s:%s%s\">here</A>.\r\n",
		       $ENV{'SERVER_NAME'},
		       $ENV{'SERVER_PORT'},
		       $error);
		}
	} else {
                printf("Location: %s\r\n",$error);
	if (!$head_only) {
                printf("%s\r\n",$title);
                printf("<BODY><H1>%s</H1>\r\n",$redirect_status);
                printf("This document has moved");
                printf("<A HREF=\"%s\">here</A>.\r\n",$error);
		}
	}
} elsif ($redirect_number == 400) {
	printf("\r\n");
	if (!$head_only) {
		printf("%s\r\n",$title);
		printf("<BODY><H1>%s</H1>\r\n",$redirect_status);
		printf("Your client sent a request that this server didn't");
		printf(" understand.<br><b>Reason:</b> %s\r\n",$error);
	}
} elsif ($redirect_number == 401) {
	printf("WWW-Authenticate: %s\r\n",$error);
	printf("\r\n");
	if (!$head_only) {
		printf("%s\r\n",$title);
		printf("<BODY><H1>%s</H1>\r\n",$redirect_status);
		printf("Browser not authentication-capable or ");
		printf("authentication failed.\r\n");
	}
} elsif ($redirect_number == 403) {
	printf("\r\n");
        if (!$head_only) {
                printf("%s\r\n",$title);
                printf("<BODY><H1>%s</H1>\r\n",$redirect_status);
                printf("Your client does not have permission to get");
                printf("URL:%s from this server.\r\n",$ENV{'REDIRECT_URL'});
        }
} elsif ($redirect_number == 404) {
	printf("\r\n");
	if (!$head_only) {
		printf("%s\r\n",$title);
		printf("<BODY><H1>%s</H1>\r\n",$redirect_status);
		printf("The requested URL:<code>%s</code> ",
			$ENV{'REDIRECT_URL'});
		printf("was not found on this server.\r\n");
	}
} elsif ($redirect_number == 500) {
        printf("\r\n");
        if (!$head_only) {
                printf("%s\r\n",$title);
                printf("<BODY><H1>%s</H1>\r\n",$redirect_status);
                printf("The server encountered an internal error or ");
                printf("misconfiguration and was unable to complete your ");
                printf("request \"<code>%s</code>\"\r\n",$redirect_request);
        }
} elsif ($redirect_number == 501) {
	printf("\r\n");
	if (!$head_only) {
		printf("%s\r\n",$title);
		printf("<BODY><H1>%s</H1>\r\n",$redirect_status);
		printf("The server is unable to perform the method ");
		printf("<b>%s</b> at this time.",$redirect_method);
	}
} else {
	printf("\r\n");
	if (!$head_only) {
		printf("%s\r\n",$title);
		printf("<BODY><H1>%s</H1>\r\n",$redirect_status);
	}
}

if (!$head_only) {
	printf("<p>The following might be useful in determining the problem:");
	printf("<pre>\r\n");
	open(ENV,"env|");
	while (<ENV>) {
		printf("$_");
	}
	close(ENV);
	printf("</pre>\r\n<hr>");
	printf("<A HREF=\"http://%s:%s/\"><img alt=\"[Back to Top]\" src=\"/images/back.gif\"> Back to Root of Server</A>\r\n",
		$ENV{'SERVER_NAME'},$ENV{'SERVER_PORT'});
	printf("<hr><i><a href=\"mailto:webmaster\@%s\">webmaster\@%s</a></i> / ",
		$ENV{'SERVER_NAME'},$ENV{'SERVER_NAME'});
	printf("<i><a href=\"mailto:httpd\@ncsa.uiuc.edu\">httpd\@ncsa.uiuc.edu</a></i>");
	printf("</BODY>\r\n");
}
