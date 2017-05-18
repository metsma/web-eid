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
APPDIR = $(TMPROOT)/Applications
LIBDIR = $(TMPROOT)/Library/Web\ eID
APP = $(APPDIR)/Web\ eID.app

$(APP): app
	mkdir -p $(APP)
	cp -R src/Web\ eID.app/ $(APP)
	cp src/nm-bridge/web-eid-bridge $(APP)/Contents/MacOS/web-eid-bridge
	$(QTPATH)/bin/macdeployqt $(APP) -always-overwrite

codesign: $(APP)
	# Possibly add "--entitlements macos/web-eid.entitlements" for sanboxing, once "osascript" replacement exists
	codesign -f --deep -s "Developer ID Application: $(SIGNER)" $(APP)

web-eid-app.pkg: $(APP)
	# Copy necessary files
	mkdir -p $(TMPROOT)$(CHROME_HOSTS) $(TMPROOT)$(FIREFOX_HOSTS) $(TMPROOT)$(EXTERNAL_PATH)
	cp -v macos/org.hwcrypto.native.json $(TMPROOT)$(CHROME_HOSTS)
	cp -v macos/org.hwcrypto.native.firefox.json $(TMPROOT)$(FIREFOX_HOSTS)/org.hwcrypto.native.json
	cp -v fmpfihjoladdfajbnkdfocnbcehjpogi.json $(TMPROOT)$(EXTERNAL_PATH)

	# Create the uninstaller
	mkdir -p $(LIBDIR)
	cp macos/uninstall.sh $(LIBDIR)
	osacompile -o $(APPDIR)/Web\ eID\ Uninstaller.app macos/uninstaller.applescript

	# Remove relocate flag from app bundle
	# TODO: check what to do with free-form app
	pkgbuild --analyze  --root $(TMPROOT) web-eid-app.plist
	/usr/libexec/PlistBuddy -c "Set :0:BundleIsRelocatable false" web-eid-app.plist

	pkgbuild --version $(VERSION) \
	--root $(TMPROOT) \
	--identifier com.web-eid.app \
	--component-plist web-eid-app.plist \
        --install-location / \
	$@

scripts.pkg: macos/installer-scripts/*
	pkgbuild --nopayload --scripts macos/installer-scripts --id com.web-eid.scripts --version $(VERSION) $@

pkg: Web-eID_$(VERSION)-unsigned.pkg
Web-eID_$(VERSION)-unsigned.pkg: web-eid-app.pkg scripts.pkg
	productbuild --distribution macos/web-eid.dist.xml \
	--package-path . \
	--resources macos \
	--scripts macos/installer-scripts \
	$@


signed: Web-eID_$(VERSION).pkg
.PHONY: Web-eID_$(VERSION).pkg
Web-eID_$(VERSION).pkg: pkg
	productsign --sign "Developer ID Installer: $(SIGNER)" Web-eID_$(VERSION)-unsigned.pkg $@

dmg: Web-eID.dmg
Web-eID.dmg: Web-eID.pkg
	hdiutil create $@ -ov -volname "Web eID installer" -srcfolder $<
