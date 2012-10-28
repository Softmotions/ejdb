#! /bin/sh

#================================================================
# printenv.cgi
# Print CGI environment variables
#================================================================


# set variables
LANG=C
LC_ALL=C
export LANG LC_ALL


# output the result
printf 'Content-Type: text/plain\r\n'
printf '\r\n'

printenv | sort


# exit normally
exit 0



# END OF FILE
