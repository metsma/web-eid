# Copyright (C) 2017 Martin Paljak

# This is the Makefile for OSX/Linux. See Makefile for Windows NMake.
include VERSION.mk
UNAME := $(shell uname)

# Default target
default: app

QT_SELECT ?= 5
export QT_SELECT

# include platform-specific makefile for the package
ifeq ($(UNAME),Linux)
include linux/Makefile
else ifeq ($(UNAME),Darwin)
include macos/Makefile
endif

QMAKE ?= qmake

app:
	$(QMAKE) -o QMakefile
	$(MAKE) -f QMakefile

# Tests must be re-thought
#test:
	# wildcard will resolve to an empty string with a missing file
	# so that OSX will not run with xvfb
#	$(wildcard /usr/bin/xvfb-run) python tests/pipe-test.py -v

release:
	# Make sure we are on master branch
	git checkout master
	# Make sure we have the latest code
	git pull --rebase
	# Check that this version has not been tagged yet
	! git tag -l | grep -q $(VERSION)
	# Tag version
	git tag $(VERSION) -as -m "Release version $(VERSION)"
	# Push to Github
	git push --tags origin master

clean:
	git clean -dfx

# Make the targzip for the native components
dist:
	git-archive-all web-eid-`git describe --tags --always`.tar.gz
