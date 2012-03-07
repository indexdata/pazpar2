PROJECT=$1
VERSION=$2
TYPE=tar.gz

if [ "$VERSION" == "" ] ; then 
    echo buildscript  project version
    exit 1
fi
# if (You need to have a rpmbuild directory - Adam has a script in git-tools )
# (You need to have .rpmmacros - edit it to point to your rpmbuild)
if cd ~/rpmbuild/SOURCES ; then 
    if tar xzf $PROJECT-$VERSION.$TYPE ; then
	cd $PROJECT-$VERSION
	if rpmbuild -ba $PROJECT.spec ; then 
	    echo success. Do upload, and update repo
	else
	    echo failed to rpmbuild $PROJECT
	fi

    else
	echo failed tar xzf $PROJECT-$VERSION.tgz
    fi
else
    echo "Unable to CD"
    exit 1;
fi
