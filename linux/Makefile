BUILD_NUMBER ?= 1
include ../VERSION.mk

hwcrypto-native:
	$(MAKE) -C ../host-qt
	cp ../host-qt/hwcrypto-native .

pkg:
	test -f /etc/debian_version && $(MAKE) pkg-deb; exit 0
	test -f /etc/redhat-release && $(MAKE) pkg-rpm; exit 0

pkg-rpm:
	(cd .. && sed -i 's/VERSION/$(VERSION)/g' hwcrypto-native.spec && fedpkg --release f25 local)

pkg-deb: hwcrypto-native
	rm -f ../debian/changelog
	(cd .. && dch --create -M --package hwcrypto-native --force-distribution --distribution `lsb_release -cs` --newversion $(VERSION)-$(BUILD_NUMBER)`lsb_release -cs`  "Publish $(VERSION) from Git `git describe --tags --always`" && fakeroot debian/rules binary)

clean:
	git clean -dfx ..
