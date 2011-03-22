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
    [--outfile=OUTFILE]   1
EOF
    exit 1
}
H=http://localhost:9004/search.pz2
SERVICE=""
SETTINGS=""
QUERY=water
OF=1
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
	-*)
	  usage
	  ;;
    esac
    shift
done
wget -q -O $OF.init.xml "$H/?command=init${SERVICE}"
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
    wget -q -O $OF.settings.xml "$H?command=settings&session=$S&${SETTINGS}"
fi
wget -q -O $OF.search.xml "$H?command=search&query=$QUERY&session=$S"
sleep 1
wget -q -O $OF.show.xml "$H?command=show&session=$S&sort=relevance&start=0&num=100&block=1"
exit 0

# Local Variables:
# mode:shell-script
# sh-indentation: 2
# sh-basic-offset: 4
# End:
