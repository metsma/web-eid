# Get version information
include ../VERSION.mk

# We need Qt
QTPATH ?= $(HOME)/Qt/5.8/clang_64
PATH := $(QTPATH)/bin:$(PATH)

# Where to place files
EXTERNAL_PATH = /Library/Application\ Support/Google/Chrome/External\ Extensions
CHROME_HOSTS = /Library/Google/Chrome/NativeMessagingHosts
FIREFOX_HOSTS = /Library/Application\ Support/Mozilla/NativeMessagingHosts
FFEXTENSION_PATH = /Library/Application\ Support/Mozilla/Extensions/\{ec8030f7-c20a-464f-9b0e-13a3a9e97384\}

TMPROOT = $(CURDIR)/tmp
TARGETFOLDER = $(TMPROOT)/Library/Web\ eID
APP = $(TARGETFOLDER)/hwcrypto-native.app

$(APP):
	$(MAKE) -C ../host-qt
	mkdir -p $(TARGETFOLDER)
	cp -R ../host-qt/hwcrypto-native.app $(TARGETFOLDER)
	$(QTPATH)/bin/macdeployqt $(APP) -always-overwrite

codesign: $(APP)
	codesign -f --deep -s "Developer ID Application: $(SIGNER)" $(APP)

clean:
	git clean -dfx

hwcrypto-native.pkg: $(APP)
	# Copy necessary files
	mkdir -p $(TMPROOT)$(CHROME_HOSTS) $(TMPROOT)$(FIREFOX_HOSTS) $(TMPROOT)$(EXTERNAL_PATH)
	cp -v org.hwcrypto.native.json $(TMPROOT)$(CHROME_HOSTS)
	cp -v org.hwcrypto.native.firefox.json $(TMPROOT)$(FIREFOX_HOSTS)/org.hwcrypto.native.json
	cp -v ../fmpfihjoladdfajbnkdfocnbcehjpogi.json $(TMPROOT)$(EXTERNAL_PATH)

	# Create the uninstaller
	cp uninstall.sh $(TARGETFOLDER)/uninstall.sh
	osacompile -o $(TARGETFOLDER)/Web\ eID\ Uninstaller.app uninstaller.applescript

	# Remove relocate flag from app bundle
	pkgbuild --analyze  --root $(TMPROOT) hwcrypto-native.plist
	/usr/libexec/PlistBuddy -c "Set :0:BundleIsRelocatable false" hwcrypto-native.plist

	# Store version in a convenient .plist
	/usr/libexec/PlistBuddy -c "Add Version string $(VERSION)" $(TARGETFOLDER)/version.plist

	pkgbuild --version $(VERSION) \
	--root $(TMPROOT) \
	--identifier org.hwcrypto.native \
	--component-plist hwcrypto-native.plist \
        --install-location / \
	$@

scripts.pkg: installer-scripts/*
	pkgbuild --nopayload --scripts installer-scripts --id org.hwcrypto.scripts --version $(VERSION) $@

pkg: Web-eID-$(VERSION)-unsigned.pkg
Web-eID-$(VERSION)-unsigned.pkg: hwcrypto-native.pkg scripts.pkg
	productbuild --distribution hwcrypto-native.dist.xml \
	--package-path . \
	--resources . \
	--scripts installer-scripts \
	../$@


signed: Web-eID-$(VERSION).pkg
.PHONY: Web-eID-$(VERSION).pkg
Web-eID-$(VERSION).pkg:
	rm -f ../*.pkg *.pkg
	$(MAKE) codesign
	$(MAKE) pkg
	productsign --sign "Developer ID Installer: $(SIGNER)" ../Web-eID-$(VERSION)-unsigned.pkg ../$@

dmg: Web-eID.dmg
Web-eID.dmg: Web-eID.pkg
	hdiutil create ../$@ -ov -volname "Web eID installer" -srcfolder ../$<
