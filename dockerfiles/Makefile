#!/bin/bash
GIT_REPOSITORY=git@bitbucket.org:martinpaljak/hwcrypto-native
VENDOR=martinpaljak
BRANCH=websocket
DISTROS = debian-testing ubuntu-16.10 ubuntu-17.04 ubuntu-17.10 fedora-25

prepare:
	for f in $(DISTROS); do \
	  docker build -t web-eid-$$f-builder web-eid-$$f-builder; \
	done

tag:
	for f in $(DISTROS); do \
	  docker tag web-eid-$$f-builder $(VENDOR)/web-eid-$$f-builder; \
	done

push:
	for f in $(DISTROS); do \
	  docker push $(VENDOR)/web-eid-$$f-builder; \
	done

build:
	cp ~/.ssh/bitbucket/* .
	for f in $(DISTROS); do \
	  docker run -e GIT_REPOSITORY=$(GIT_REPOSITORY) -e TRAVIS_COMMIT=$(BRANCH) -v $${PWD}:/artifacts $(VENDOR)/web-eid-$$f-builder; \
	done

clean:
	rm -f *.deb *.rpm
