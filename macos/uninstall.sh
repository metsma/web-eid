#!/bin/bash
if [ "$(id -u)" != "0" ]; then
   echo "This script must be run as root!" 1>&2
   echo "Use \"Web eID Uninstaller.app\"" 1>&2
   exit 1
fi

# Remove browser registrations
rm -f /Library/Application\ Support/Google/Chrome/External\ Extensions/fmpfihjoladdfajbnkdfocnbcehjpogi.json
rm -f /Library/Google/Chrome/NativeMessagingHosts/org.hwcrypto.native.json
rm -f /Library/Application\ Support/Mozilla/NativeMessagingHosts/org.hwcrypto.native.json
# TODO: rm -f /Users/*/org.hwcrypto.native.json

# Remove installed files
rm -rf /Library/Web\ eID
rm -rf /Applications/Web\ eID.app

# delete receipts
rm -f /var/db/receipts/org.hwcrypto.native.bom
rm -f /var/db/receipts/org.hwcrypto.native.plist
rm -f /var/db/receipts/com.web-eid.app.bom
rm -f /var/db/receipts/com.web-eid.app.plist

echo "Web eID has been removed from your system. See you again!"
