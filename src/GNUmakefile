# Construct version
BUILD_NUMBER ?= 0
include ../VERSION.mk
QMAKE ?= qmake

# We don't NEED to have a four point version on unixes, so don't use it.
all:
	$(QMAKE) VERSION=$(VERSION) -config release
	$(MAKE) -f Makefile

clean:
	$(MAKE) -f Makefile distclean
