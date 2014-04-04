Name:		profanity
Version:	0.4.0
Release:	2%{?dist}
Summary:	A console based XMPP client

Group:		Application/System
License:	GPL
URL:		http://www.profanity.im/
Source0:	%{name}-%{version}.tar.gz

BuildRequires:	libstrophe-devel
BuildRequires:	libcurl-devel
BuildRequires:	ncurses-devel
BuildRequires:	openssl-devel
BuildRequires:	glib2-devel
BuildRequires:	expat-devel
BuildRequires:	libotr-devel
Requires:	libstrophe
Requires:	libcurl
Requires:	ncurses-libs
Requires:	openssl
Requires:	glib2
Requires:	expat
Requires:	libotr

%description
Profanity is a console based XMPP client written in C using ncurses and libstrophe, inspired by Irssi.

%prep
%setup -q

%build
%configure
make %{?_smp_mflags}

%install
make install DESTDIR=%{buildroot}

%files
%{_bindir}/profanity
%doc %{_datadir}/man/man1/profanity.1.gz

%changelog
