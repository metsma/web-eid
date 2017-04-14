# This file is included by the master Makefile in ..
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
APP = $(TARGETFOLDER)/web-eid.app

$(APP):
	$(MAKE) -C src
	mkdir -p $(TARGETFOLDER)
	cp -R src/web-eid.app $(TARGETFOLDER)
	macdeployqt $(APP) -always-overwrite

codesign: $(APP)
	codesign -f --deep -s "Developer ID Application: $(SIGNER)" $(APP)

web-eid.pkg: $(APP)
	# Copy necessary files
	mkdir -p $(TMPROOT)$(CHROME_HOSTS) $(TMPROOT)$(FIREFOX_HOSTS) $(TMPROOT)$(EXTERNAL_PATH)
	cp -v macos/org.hwcrypto.native.json $(TMPROOT)$(CHROME_HOSTS)
	cp -v macos/org.hwcrypto.native.firefox.json $(TMPROOT)$(FIREFOX_HOSTS)/org.hwcrypto.native.json
	cp -v fmpfihjoladdfajbnkdfocnbcehjpogi.json $(TMPROOT)$(EXTERNAL_PATH)

	# Create the uninstaller
	cp macos/uninstall.sh $(TARGETFOLDER)/uninstall.sh
	osacompile -o $(TARGETFOLDER)/Web\ eID\ Uninstaller.app macos/uninstaller.applescript

	# Remove relocate flag from app bundle
	pkgbuild --analyze  --root $(TMPROOT) web-eid.plist
	/usr/libexec/PlistBuddy -c "Set :0:BundleIsRelocatable false" web-eid.plist

	# Store version in a convenient .plist
	/usr/libexec/PlistBuddy -c "Add Version string $(VERSION)" $(TARGETFOLDER)/version.plist

	pkgbuild --version $(VERSION) \
	--root $(TMPROOT) \
	--identifier org.hwcrypto.native \
	--component-plist web-eid.plist \
        --install-location / \
	$@

scripts.pkg: macos/installer-scripts/*
	pkgbuild --nopayload --scripts macos/installer-scripts --id org.hwcrypto.scripts --version $(VERSION) $@

pkg: Web-eID_$(VERSION)-unsigned.pkg
Web-eID_$(VERSION)-unsigned.pkg: web-eid.pkg scripts.pkg
	productbuild --distribution macos/web-eid.dist.xml \
	--package-path . \
	--resources macos \
	--scripts macos/installer-scripts \
	$@


signed: Web-eID_$(VERSION).pkg
.PHONY: Web-eID_$(VERSION).pkg
Web-eID_$(VERSION).pkg:
	rm -f *.pkg
	$(MAKE) codesign
	$(MAKE) pkg
	productsign --sign "Developer ID Installer: $(SIGNER)" Web-eID_$(VERSION)-unsigned.pkg $@

dmg: Web-eID.dmg
Web-eID.dmg: Web-eID.pkg
	hdiutil create $@ -ov -volname "Web eID installer" -srcfolder $<
