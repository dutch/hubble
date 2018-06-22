%define version 0.1


Name:           hubble
Version:        %{version}
Release:        1%{?dist}
Summary:        Continuous integration client for GitHub
License:        GPLv3+
URL:            https://github.com/dutch/%{name}
Source0:        https://github.com/dutch/%{name}/archive/master.tar.gz#/%{name}-%{version}-%{release}.tar.gz
BuildRequires:  gcc
BuildRequires:  make
BuildRequires:  autoconf
BuildRequires:  autotools-latest
%{?systemd_requires}
BuildRequires:  pkgconfig(systemd)


%description
Hubble is a CI client for GitHub.


%prep
%autosetup -n %{name}-master


%build
scl enable autotools-latest 'autoreconf -i'
%configure
make %{?_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT
%make_install


%post
%systemd_post %{name}.service


%preun
%systemd_preun %{name}.service


%postun
%systemd_postun_with_restart %{name}.service


%files
%license COPYING
%{_bindir}/%{name}
%{_libexecdir}/%{name}/responder
%{_unitdir}/%{name}.service


%changelog
