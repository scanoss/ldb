Name:       ldb
Version:    LDB_VERSION
Release:    1%{?dist}
Summary:    SCANOSS LDB
License:    GPLv2
BuildArch:  x86_64

%description
The LDB (Linked-list database) is a headless database management system focused in single-key, read-only application on vast amounts of data, while maintaining a minimal footprint and keeping system calls to a bare minimum. Information is structured using linked lists.

%prep

%build

%install
mkdir -p %{buildroot}/%{_bindir}
mkdir -p %{buildroot}/%{_libdir}
install -m 0755 %{name} %{buildroot}/%{_bindir}/%{name}
install -m 0755 libldb.so %{buildroot}/%{_libdir}/libldb.so

%files
%{_bindir}/%{name}
%{_libdir}/libldb.so

%changelog