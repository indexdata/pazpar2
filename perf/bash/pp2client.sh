#!/bin/sh

usage()
{
    cat <<EOF
Usage: pp2client.sh [OPTIONS]
Options:                  Default values
    [--prefix=URLPREFIX]  http://localhost:9004/search.pz2
    [--query=QUERY]       water
    [--service=SERVICE]
    [--settings=SETTINGS]
    [--outfile=OUTFILE]
    [--timed]
EOF
    exit 1
}
H=http://localhost:9004/search.pz2
SERVICE=""
SETTINGS=""
QUERY=water
OF=1
TIME=""

if test $# -eq 0; then 
    usage
    exit 0;
fi

while test $# -gt 0; do
    case "$1" in
        -*=*) optarg=`echo "$1" | sed 's/[-_a-zA-Z0-9]*=//'` ;;
        *) optarg= ;;
    esac
    case $1 in
        --prefix=*)
          H=$optarg
          ;;
        --query=*)
	  QUERY=$optarg
	  ;;
	--service=*)
	  SERVICE="&service=$optarg"
	  ;;
	--settings=*)
	  SETTINGS="$optarg"
	  ;;
	--outfile=*)
	  OF=$optarg
	  ;;
	--timed)
	  TIME="yes"
	  ;;
	-*)
	  usage
	  ;;
    esac
    shift
done

if [ "$TIME" != "" ] ; then
    /usr/bin/time --format "$OF, init, %e" wget -q -O ${TMP_DIR}$OF.init.xml "$H/?command=init${SERVICE}"  2> ${TMP_DIR}$OF.init.time
else
    wget -q -O ${TMP_DIR}$OF.init.xml "$H/?command=init${SERVICE}" 
fi

R="$?"
if [ "$R" != 0 ]; then
    if [ "$R" = "4" ]; then    
	echo "wget returned network error. Maybe Pazpar2 is not running at"
	echo "$H"
	exit 4
    fi
    echo "wget failed. Exit code $R"
    exit 1
fi
S=`xsltproc get_session.xsl $OF.init.xml`
if [ -n "$SETTINGS" ] ; then
    if [ "$TIME" != "" ] ; then
	/usr/bin/time --format "$OF, settings, %e" wget -q -O ${TMP_DIR}$OF.settings.xml "$H?command=settings&session=$S&${SETTINGS}" 2> ${TMP_DIR}$OF.settings.time
    else
	wget -q -O ${TMP_DIR}$OF.settings.xml "$H?command=settings&session=$S&${SETTINGS}" 
    fi
fi

if [ "$TIME" != "" ] ; then
    /usr/bin/time --format "$OF, search, %e" wget -q -O ${TMP_DIR}$OF.search.xml "$H?command=search&query=$QUERY&session=$S" 2> ${TMP_DIR}$OF.search.time
else
    wget -q -O ${TMP_DIR}$OF.search.xml "$H?command=search&query=$QUERY&session=$S"
fi
sleep 1
if [ "$TIME" != "" ] ; then
    /usr/bin/time --format "$OF, show, %e" wget -q -O ${TMP_DIR}$OF.show.xml "$H?command=show&session=$S&sort=relevance&start=0&num=100&block=1" 2> ${TMP_DIR}$OF.show.time
else
    wget -q -O ${TMP_DIR}$OF.show.xml "$H?command=show&session=$S&sort=relevance&start=0&num=100&block=1"
fi
wget -q -O ${TMP_DIR}$OF.bytarget.xml "$H?command=bytarget&session=$S"
exit 0

# Local Variables:
# mode:shell-script
# sh-indentation: 2
# sh-basic-offset: 4
# End:
