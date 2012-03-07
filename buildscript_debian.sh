PROJECT=$1
TYPE=tar.gz
RPMBUILD_HOST=centos5

if ~/proj/git-tools/id-deb-build/check-for-dummy.sh ; then 
    echo "Version $VERSION OK"
    . IDMETA
else 
    echo "Mismatch in IDMETA and debian/changelog"
fi

if [ "$PROJECT" == "" ] ; then 
    echo buildscript  project [TYPE]
    exit 1
fi

if [ "$2" != ""]
    TYPE=$2
fi

if ./buildconf.sh -d ; then 
    if make dist ; then
	if scp ${PROJECT}-${VERSION}.${TYPE} ${RPMBUILD_HOST}:rpmbuild/SOURCES ; then 
	    echo uploaded to ${RPMBUILD_HOST}. Run buildscript_rpm.sh $PROJECT $VERSION $TYPE from here
	    sleep 5
	else
	    echo failed to upload to ${RPMBUILD_HOST}
	    exit 1
	fi
	# Now debian (p)build
	if ~/proj/git-tools/id-deb-build/id-pbuild.sh ; then
	    echo Upload and update repo
	else
	    echo "Failed to build $PROJECT $VERSION
	    exit 
	fi
    else
	echo failed to make dist
    fi
else
    echo "buildconf -d failed"
    exit 1;
fi
