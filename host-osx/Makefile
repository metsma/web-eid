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

EXTERNAL_PATH = /Library/Application\ Support/Google/Chrome/External\ Extensions
NATIVE_PATH = /Library/Google/Chrome/NativeMessagingHosts
FIREFOX_PATH = /Library/Application\ Support/Mozilla/NativeMessagingHosts
FFEXTENSION_PATH = /Library/Application\ Support/Mozilla/Extensions/\{ec8030f7-c20a-464f-9b0e-13a3a9e97384\}

BUILD_NUMBER ?= 0
include ../VERSION.mk

OPENSSL ?= /usr/local/Cellar/openssl/1.0.2j
TMPROOT = $(CURDIR)/tmp
TARGET = $(TMPROOT)/Library/Google/Chrome/NativeMessagingHosts/hwcrypto-native.app
PKG = hwcrypto-native.pkg

PROJ = xcodebuild -project hwcrypto-native.xcodeproj -configuration Release VERSION=$(VERSION) BUILD_NUMBER=$(BUILD_NUMBER) OPENSSL=$(OPENSSL)

$(TARGET): *.mm $(OPENSSL)
	$(PROJ) install DSTROOT=$(TMPROOT)

codesign: $(TARGET)
	codesign -f --entitlements hwcrypto-native.entitlements -s "Developer ID Application: $(SIGNER)" "$(TARGET)"

clean:
	$(PROJ) clean
	git clean -dfx

$(PKG): $(TARGET)
	pkgbuild --version $(VERSION) \
		--root $(TMPROOT) \
		--identifier org.hwcrypto.native \
                --install-location / \
		$(PKG)

pkg: $(PKG)

signed: codesign
	pkgbuild --version $(VERSION) \
		--root $(TMPROOT) \
		--identifier org.hwcrypto.native \
                --install-location / \
                --sign "Developer ID Installer: $(SIGNER)" \
		$(PKG)


Web-eID.pkg: product

scripts.pkg: installer-scripts/*
	pkgbuild --nopayload --scripts installer-scripts --id org.hwcrypto.scripts --sign "Developer ID Installer: $(SIGNER)" scripts.pkg

product: signed scripts.pkg
	productbuild --distribution hwcrypto-native.dist.xml \
	--package-path . \
	--resources . \
	--sign "Developer ID Installer: $(SIGNER)" Web-eID.pkg

$(OPENSSL):
	test -e openssl || git clone --depth=1 https://github.com/openssl/openssl.git -b OpenSSL_1_0_2-stable
	(cd openssl \
	&& KERNEL_BITS=64 ./config --prefix=$(OPENSSL) -mmacosx-version-min=10.9 no-shared no-ssl2 no-idea no-dtls no-psk no-srp no-apps \
	&& make depend \
	&& make \
	&& make install_sw)

# helpers

install:
	mkdir -p $(NATIVE_PATH) $(FIREFOX_PATH) $(FFEXTENSION_PATH) $(EXTERNAL_PATH)
	cp -rv build/Release/hwcrypto-native.app $(NATIVE_PATH)
	cp -v org.hwcrypto.native.json $(NATIVE_PATH)
	cp -v org.hwcrypto.native.firefox.json $(FIREFOX_PATH)/org.hwcrypto.native.json
	# NOT installed, untill hwcrypto/hwcrypto-extension#12 is fixed
	#cp -v ../{443830f0-1fff-4f9a-aa1e-444bafbc7319}.xpi $(FFEXTENSION_PATH)
	cp -v ../fmpfihjoladdfajbnkdfocnbcehjpogi.json $(EXTERNAL_PATH)

uninstall:
	rm -f $(EXTERNAL_PATH)/fmpfihjoladdfajbnkdfocnbcehjpogi.json
	rm -f $(NATIVE_PATH)/org.hwcrypto.native.json
	rm -f $(FIREFOX_PATH)/org.hwcrypto.native.json
	# NOT installed, untill hwcrypto/hwcrypto-extension#12 is fixed
	#rm -f $(FFEXTENSION_PATH)/{443830f0-1fff-4f9a-aa1e-444bafbc7319}.xpi
	rm -rf $(NATIVE_PATH)/hwcrypto-native.app

test:
	python ../host-test/stateless-test.py $(TARGET)/Contents/MacOS/hwcrypto-native
