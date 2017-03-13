#
# Chrome Token Signing Native Host
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
#

# This is the Makefile for OSX/Linux. See Makefile for Windows NMake.
include VERSION.mk
UNAME :=$(shell uname)

default: $(DEFAULT) pkg

# map uname output to subfolder and run make there.
.DEFAULT:
	$(MAKE) -C $(subst Darwin,macos,$(subst Linux,linux,$(UNAME))) $@

test:
	# wildcard will resolve to an empty string with a missing file
	# so that OSX will not run with xvfb
	$(wildcard /usr/bin/xvfb-run) python tests/pipe-test.py -v

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

# Make the targzip for the native components
dist:
	git-archive-all hwcrypto-native-`git describe --tags --always`.tar.gz
