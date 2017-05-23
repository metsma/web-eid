# This file is included by the parent Makefile in ..

.PHONY: pkg
pkg: app
	test -f /etc/debian_version && $(MAKE) pkg-deb; exit 0
	test -f /etc/redhat-release && $(MAKE) pkg-rpm; exit 0

pkg-rpm:
	sed -i 's/VERSION/$(VERSION)/g' web-eid.spec
	fedpkg --release f25 local

pkg-deb:
	git clean -dffx debian
	dch --create -M --package web-eid --force-distribution --distribution `lsb_release -cs` --newversion $(VERSION)-$(BUILD_NUMBER)`lsb_release -cs`  "Publish $(VERSION) from Git `git describe --tags --always`"
	+fakeroot debian/rules binary