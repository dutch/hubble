%define version 0.1


Name:           hubble
Version:        %{version}
Release:        1%{?dist}
Summary:        Continuous integration client for GitHub
License:        GPLv3+
URL:            https://github.com/dutch/%{name}
Source0:        https://github.com/dutch/%{name}/archive/master.tar.gz#/%{name}-%{version}-%{release}.tar.gz
BuildRequires:  gcc


%description
Hubble is a CI client for GitHub.


%prep
%setup -q


%build
%configure
make %{?_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT
%make_install


%files
%doc COPYING
%{_bindir}/%{name}
%{_libexecdir}/%{name}/responder


%changelog
