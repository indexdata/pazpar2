Summary: Metasearcher
Name: pazpar2
Version: 1.6.3
Release: 1indexdata
License: GPL
Group: Applications/Internet
Vendor: Index Data ApS <info@indexdata.dk>
Source: pazpar2-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-root
BuildRequires: libyaz4-devel >= 4.2.17
Packager: Adam Dickmeiss <adam@indexdata.dk>
URL: http://www.indexdata.com/pazpar2
Summary: pazpar2 daemon
Requires: libyaz4 >= 4.2.17

%description
Pazpar2 is a high-performance, user interface-independent, data
model-independent metasearching middleware featuring merging, relevance
ranking, record sorting, and faceted results.

%package -n pazpar2-js
Summary: pazpar2 JS
Group: Data
Requires: pazpar2

%post
if [ $1 = 1 ]; then
	/sbin/chkconfig --add pazpar2
	/sbin/service pazpar2 start > /dev/null 2>&1
else
	/sbin/service pazpar2 restart > /dev/null 2>&1
fi
%preun
if [ $1 = 0 ]; then
	/sbin/service pazpar2 stop > /dev/null 2>&1
	/sbin/chkconfig --del pazpar2
fi

%description -n pazpar2-js
This package includes the Java Script library pz2.js. It also adds an
Alias for Apache2 so that this library and other demo portals are
available.

%posttrans -n pazpar2-js
if [ -d /etc/httpd/conf.d ]; then
	ln -sf /etc/pazpar2/ap2pazpar2-js.cfg /etc/httpd/conf.d/pazpar2-js.conf
fi
%preun -n pazpar2-js
if [ $1 = 0 ]; then
	if [ -L /etc/httpd/conf.d/pazpar2-js.conf ]; then
		rm /etc/httpd/conf.d/pazpar2-js.conf
	fi
fi
%package -n pazpar2-doc
Summary: pazpar2 documentation
Group: Data

%description -n pazpar2-doc
This package includes documentation for Pazpar2 - the metasearcher.

%prep
%setup

%build

CFLAGS="$RPM_OPT_FLAGS" \
 ./configure --prefix=%{_prefix} --libdir=%{_libdir} --mandir=%{_mandir} \
	--with-yaz=/usr/bin
make CFLAGS="$RPM_OPT_FLAGS"

%install
rm -fr ${RPM_BUILD_ROOT}
make prefix=${RPM_BUILD_ROOT}/%{_prefix} mandir=${RPM_BUILD_ROOT}/%{_mandir} \
	libdir=${RPM_BUILD_ROOT}/%{_libdir} install
mkdir -p ${RPM_BUILD_ROOT}/etc/pazpar2
mkdir -p ${RPM_BUILD_ROOT}/etc/pazpar2/settings
mkdir -p ${RPM_BUILD_ROOT}/etc/pazpar2/services-enabled
mkdir -p ${RPM_BUILD_ROOT}/etc/pazpar2/services-available
cp etc/server.xml ${RPM_BUILD_ROOT}/etc/pazpar2/
cp etc/default.xml ${RPM_BUILD_ROOT}/etc/pazpar2/services-available/
cp etc/services/*.xml ${RPM_BUILD_ROOT}/etc/pazpar2/services-available/
cp etc/settings/*.xml ${RPM_BUILD_ROOT}/etc/pazpar2/settings/
cp etc/*.xsl ${RPM_BUILD_ROOT}/etc/pazpar2/
mkdir -p ${RPM_BUILD_ROOT}/etc/rc.d/init.d
install -m755 rpm/pazpar2.init ${RPM_BUILD_ROOT}/etc/rc.d/init.d/pazpar2
echo "Alias /pazpar2 /usr/share/pazpar2" >${RPM_BUILD_ROOT}/etc/pazpar2/ap2pazpar2-js.cfg
mkdir -p ${RPM_BUILD_ROOT}/etc/logrotate.d
install -m644 rpm/pazpar2.logrotate ${RPM_BUILD_ROOT}/etc/logrotate.d/pazpar2

%clean
rm -fr ${RPM_BUILD_ROOT}

%files
%defattr(-,root,root)
%doc README LICENSE NEWS
%{_sbindir}/pazpar2
%dir %{_sysconfdir}/pazpar2
%dir %{_sysconfdir}/pazpar2/settings
%dir %{_sysconfdir}/pazpar2/services-enabled
%dir %{_sysconfdir}/pazpar2/services-available
%config %{_sysconfdir}/pazpar2/*.xml
%config %{_sysconfdir}/pazpar2/*.xsl
%config %{_sysconfdir}/pazpar2/settings/*.xml
%config %{_sysconfdir}/pazpar2/services-available/*.xml
%config %{_sysconfdir}/rc.d/init.d/pazpar2
%config(noreplace) /etc/logrotate.d/pazpar2
%{_mandir}/man5/pazpar2*
%{_mandir}/man7/pazpar2*
%{_mandir}/man8/pazpar2*

%files -n pazpar2-js
%defattr(-,root,root)
%{_datadir}/pazpar2/js/pz2.js
%config %{_sysconfdir}/pazpar2/ap2pazpar2-js.cfg

%files -n pazpar2-doc
%defattr(-,root,root)
%{_defaultdocdir}/pazpar2
