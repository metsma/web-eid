BUILD_NUMBER ?= 1
include ../VERSION.mk

hwcrypto-native:
	$(MAKE) -C ../host-qt
	cp ../host-qt/hwcrypto-native .

pkg: hwcrypto-native
	rm -f ../debian/changelog
	(cd .. && dch --create -M --package hwcrypto-native --force-distribution --distribution `lsb_release -cs` --newversion $(VERSION)-$(BUILD_NUMBER)`lsb_release -cs`  "Publish $(VERSION) from Git `git describe --tags --always`" && fakeroot debian/rules binary)

clean:
	git clean -dfx ..
