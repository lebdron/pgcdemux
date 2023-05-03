Name:           cpgcdemux
Version:        0.1
Release:        1%{?dist}
Summary:        Tool for demuxing a DVD PGC/VID/CELL in its elementary streams

License:        LGPL-2.1+
URL:            http://cdslow.org.ru/en/cpgcdemux
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake gcc-c++

BuildRoot:      %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)

%description
CPGCDEMUX is a console port of PgcDemux by Jesus Soto.

It is a tool for demuxing a DVD PGC/VID/CELL in its elementary streams.

This port based on version 1.2.0.5 of original PgcDemux.

%prep
%setup -q

%build
%if 0%{?cmake:1}
    %cmake .
%else
    CFLAGS="${CFLAGS:-%optflags}"
    export CFLAGS
    cmake \
        -DCMAKE_VERBOSE_MAKEFILE=ON \
        -DCMAKE_INSTALL_PREFIX:PATH=%{_prefix} \
        -DCMAKE_INSTALL_LIBDIR:PATH=%{_libdir} \
        -DINCLUDE_INSTALL_DIR:PATH=%{_includedir} \
        -DLIB_INSTALL_DIR:PATH=%{_libdir} \
        -DSYSCONF_INSTALL_DIR:PATH=%{_sysconfdir} \
        -DSHARE_INSTALL_PREFIX:PATH=%{_datadir} \
        -DCMAKE_SKIP_RPATH:BOOL=ON \
        -DBUILD_SHARED_LIBS:BOOL=ON \
        .
%endif
make %{?_smp_mflags}

%install
%if 0%{?suse_version} > 0 && 0%{?suse_version} < 1200
    strip sleid0r
%endif

%if 0%{?make_install:1}
    %make_install
%else
    make install DESTDIR=%{buildroot}
%endif

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(0755, root, root)
%{_bindir}/cpgcd
%doc %dir %{_datadir}/doc/cpgcdemux
%defattr(0644, root, root)
%doc %{_datadir}/doc/cpgcdemux/README.txt
%doc %{_datadir}/doc/cpgcdemux/ReadmePgcDemux.txt

%changelog
* Sun Dec 22 2013 Vadim Druzhin <cdslow@mail.ru> - 0.1-1
- First release.

