ifeq ($(BUILD_NUMBER),)
BUILD_NUMBER = 0
endif
include ../VERSION.mk

all:
	qmake VERSION=$(VERSIONEX) -config release
	make -f Makefile

clean:
	make -f Makefile distclean

install:
	# Debian scripts expect "standard DESTDIR" to work
	make -f Makefile install INSTALL_ROOT=$(DESTDIR)

uninstall:
	make -f Makefile uninstall

debian:
	$(eval RELEASE = $(shell lsb_release -cs))
	cd ..; \
	git-archive-all ../hwcrypto-native_$(VERSIONEX).orig.tar.gz; \
	dch --distribution unstable -v $(VERSIONEX)-$(RELEASE) "Release: $(VERSIONEX)."; \
	dpkg-buildpackage
