# Construct version
BUILD_NUMBER ?= 0
QMAKE ?= qmake

# We don't NEED to have a four point version on unixes, so don't use it.
all:
	$(QMAKE) -config debug
	$(MAKE) -f Makefile

clean:
	$(MAKE) -f Makefile distclean
