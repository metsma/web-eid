Name:           hwcrypto-native
Version:        VERSION
Release:        1%{?dist}
Summary:        Use your eID card on the Web

License:        LGPLv2+
URL:            https://web-eid.com

BuildRequires:  qt5-qtbase-devel qt5-linguist pcsc-lite-devel
Requires:       pcsc-lite opensc

%description
Use your eID card on the Web

%build
make -C src QMAKE=qmake-qt5

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p %{buildroot}/%{_libexecdir}
install -p -m 755 src/hwcrypto-native %{buildroot}/%{_libexecdir}
mkdir -p %{buildroot}/etc/opt/chrome/native-messaging-hosts
install -p -m 644 linux/org.hwcrypto.native.json %{buildroot}/etc/opt/chrome/native-messaging-hosts/org.hwcrypto.native.json
sed -i 's/\/usr\/lib/\/usr\/libexec/g' %{buildroot}/etc/opt/chrome/native-messaging-hosts/org.hwcrypto.native.json
mkdir -p %{buildroot}/etc/chromium/native-messaging-hosts
install -p -m 644 linux/org.hwcrypto.native.json %{buildroot}/etc/chromium/native-messaging-hosts/org.hwcrypto.native.json
sed -i 's/\/usr\/lib/\/usr\/libexec/g' %{buildroot}/etc/chromium/native-messaging-hosts/org.hwcrypto.native.json
mkdir -p %{buildroot}/usr/lib64/mozilla/native-messaging-hosts
install -p -m 644 linux/org.hwcrypto.native.firefox.json %{buildroot}/usr/lib64/mozilla/native-messaging-hosts/org.hwcrypto.native.json
sed -i 's/\/usr\/lib/\/usr\/libexec/g' %{buildroot}/usr/lib64/mozilla/native-messaging-hosts/org.hwcrypto.native.json

%files
%{_libexecdir}/hwcrypto-native
/etc/opt/chrome/native-messaging-hosts/org.hwcrypto.native.json
/etc/chromium/native-messaging-hosts/org.hwcrypto.native.json
/usr/lib64/mozilla/native-messaging-hosts/org.hwcrypto.native.json

%license LICENSE.LGPL
