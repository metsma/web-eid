BUILD_NUMBER ?= 1
include ../VERSION.mk

hwcrypto-native:
	$(MAKE) -C ../host-qt
	cp ../host-qt/hwcrypto-native .

pkg: hwcrypto-native
	rm -f ../debian/changelog
	(cd .. && dch --create -M --package hwcrypto-native --distribution unstable --newversion $(VERSION)-$(BUILD_NUMBER)  "Release version $(VERSION)" && fakeroot debian/rules binary)

clean:
	git clean -dfx ..
