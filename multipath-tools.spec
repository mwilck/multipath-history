%define _rpmdir rpms
%define _builddir .

Summary: Tools to manage multipathed devices with the device-mapper.
Name: multipath-tools
Version: 0.2.4
Release: 1
License: GPL
Group: Utilities/System
URL: http://christophe.varoqui.free.fr
Source: /dev/null
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-buildroot
Packager: Christophe Varoqui <christophe.varoqui@free.fr>
Prefix: /
Vendor: Starving Linux Artists (tm Brian O'Sullivan)
ExclusiveOS: linux

%description
%{name} provides the tools to manage multipathed devices by
instructing the device-mapper multipath module what to do. The tools
are :
* multipath :   scan the system for multipathed devices, assembles them
                and update the device-mapper's maps
* multipathd :  wait for maps events, then execs multipath
* devmap-name : provides a meaningful device name to udev for devmaps
* kpartx :      maps linear devmaps upon device partitions, which makes
                multipath maps partionable

%prep
mkdir -p %{buildroot} %{_rpmdir}

%build
make

%install
rm -rf %{buildroot}
make DESTDIR=%{buildroot} install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%{prefix}/sbin/devmap_name
%{prefix}/sbin/multipath
%{prefix}/sbin/kpartx
%{prefix}/usr/share/man/man8/devmap_name.8.gz
%{prefix}/usr/share/man/man8/multipath.8.gz
%{prefix}/usr/bin/multipathd
%{prefix}/etc/hotplug.d/scsi/multipath.hotplug
%{prefix}/etc/init.d/multipathd


%changelog
* Sat May 14 2004 Christophe Varoqui
- Initial build.
