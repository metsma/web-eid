# This file is included by the parent Makefile in ..

src/hwcrypto-native:
	$(MAKE) -C src

.PHONY: pkg
pkg: src/hwcrypto-native
	test -f /etc/debian_version && $(MAKE) pkg-deb; exit 0
	test -f /etc/redhat-release && $(MAKE) pkg-rpm; exit 0

pkg-rpm:
	sed -i 's/VERSION/$(VERSION)/g' hwcrypto-native.spec
	fedpkg --release f25 local

pkg-deb:
	rm -f debian/changelog
	dch --create -M --package hwcrypto-native --force-distribution --distribution `lsb_release -cs` --newversion $(VERSION)-$(BUILD_NUMBER)`lsb_release -cs`  "Publish $(VERSION) from Git `git describe --tags --always`"
	fakeroot debian/rules binary
