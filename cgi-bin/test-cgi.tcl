#!/usr/local/bin/tclsh
# tcl-cgi.tcl
# robert.bagwill@nist.gov, no warranty, no rights reserved
# print out command line args, stdin, and environment variables
#
# some fixes by dl@hplyot.obspm.fr - v1.1 - Apr 11 1995
#
set envvars {SERVER_SOFTWARE SERVER_NAME GATEWAY_INTERFACE SERVER_PROTOCOL SERVER_PORT REQUEST_METHOD PATH_INFO PATH_TRANSLATED SCRIPT_NAME QUERY_STRING REMOTE_HOST REMOTE_ADDR REMOTE_USER AUTH_TYPE CONTENT_TYPE CONTENT_LENGTH HTTP_ACCEPT HTTP_REFERER HTTP_USER_AGENT}

puts "Content-type: text/html\n"
puts "<HTML>"
puts "<HEAD>"
puts "<TITLE>CGI/1.1 TCL script report:</TITLE>"
puts "</HEAD>"

puts "<BODY>"
puts "<H1>Command Line Arguments</H1>"
puts "argc is $argc. argv is $argv."
puts ""

puts "<H1>Message</H1>"
puts "<PRE>"
if {[string compare $env(REQUEST_METHOD) "POST"]==0} {
set message [split [read stdin $env(CONTENT_LENGTH)] &]
} else {
set message [split $env(QUERY_STRING) &]
}
foreach pair $message {
	set name [lindex [split $pair =] 0]
	set val [lindex [split $pair =] 1]
	regsub -all {\+} $val { } val
	# kludge to unescape some chars
	regsub -all {\%0A} $val \n\t val
	regsub -all {\%2C} $val {,} val
	regsub -all {\%27} $val {'} val
	puts "$name\t= $val"
}
puts "</PRE>"

puts "<H1>Environment Variables</H1>"
puts "<DL>"
foreach var $envvars {
	if {[info exists env($var)]} {
		puts "<DT>$var<DD>$env($var)"
		}
	}
}
puts "</DL>"
puts "</BODY>"
puts "</HTML>"
######################
# end of tcl-cgi.tcl
######################

