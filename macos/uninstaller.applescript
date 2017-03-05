tell application "System Events"
        if exists file "/Library/Web eID/uninstall.sh" then
		display alert "Web eID Uninstaller" message "This will uninstall Web eID. Please give us feedback why you uninstalled" as critical buttons {"Cancel", "Uninstall", "Go to website"} default button "Uninstall"
		set response to button returned of the result
		if response is "Go to website" then open location "https://web-eid.com/?uninstaller=macos"
		if response is "Cancel" then error number -128
                set result to do shell script "/Library/Web\\ eID/uninstall.sh" with administrator privileges
                display alert "Removal complete" message result giving up after 10
        else
                display alert "Web eID is not properly installed" message "Could not find /Library/Web eID/uninstall.sh" as critical giving up after 10
        end if
end tell
