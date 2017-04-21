Name "Web eID per-user installer"
OutFile "Web-eID_${VERSION}-local.exe"
ShowInstDetails hide
SilentInstall normal
RequestExecutionLevel user

InstallDir "$LOCALAPPDATA\Web eID"


Section
SetAutoClose true
SetOutPath "$INSTDIR"
AllowSkipFiles off
IfFileExists $INSTDIR\Web-eID.exe 0 +2
MessageBox MB_OK "If you are upgrading, make sure Web eID app is closed before continuing"

File src\release\Web-eID.exe
File src\nm-bridge\release\web-eid-bridge.exe

File windows\org.hwcrypto.native.json
File windows\org.hwcrypto.native.firefox.json

File C:\Qt\5.8\msvc2015\bin\Qt5Core.dll
File C:\Qt\5.8\msvc2015\bin\Qt5Gui.dll
File C:\Qt\5.8\msvc2015\bin\Qt5Network.dll
File C:\Qt\5.8\msvc2015\bin\Qt5PrintSupport.dll
File C:\Qt\5.8\msvc2015\bin\Qt5Widgets.dll
File C:\Qt\5.8\msvc2015\bin\Qt5WinExtras.dll
File C:\Qt\5.8\msvc2015\bin\Qt5WebSockets.dll
File C:\Qt\5.8\msvc2015\bin\libEGL.dll
File C:\Qt\5.8\msvc2015\bin\libGLESv2.dll
File C:\Qt\5.8\msvc2015\bin\D3DCompiler_47.dll
File C:\Qt\5.8\msvc2015\bin\opengl32sw.dll

File "$%VCINSTALLDIR%\redist\x86\Microsoft.VC140.CRT\msvcp140.dll"
File "$%VCINSTALLDIR%\redist\x86\Microsoft.VC140.CRT\vcruntime140.dll"

File C:\OpenSSL-Win32\bin\libeay32.dll
File C:\OpenSSL-Win32\bin\ssleay32.dll
File C:\Windows\System32\msvcr120.dll

SetOutPath "$INSTDIR\platforms"
File C:\Qt\5.8\msvc2015\plugins\platforms\qwindows.dll

SetRegView 32
WriteRegStr HKCU "SOFTWARE\Mozilla\NativeMessagingHosts\org.hwcrypto.native" '' '$INSTDIR\org.hwcrypto.native.firefox.json'
WriteRegStr HKCU "SOFTWARE\Google\Chrome\NativeMessagingHosts\org.hwcrypto.native" '' '$INSTDIR\org.hwcrypto.native.json'
SetRegView 64
WriteRegStr HKCU "SOFTWARE\Mozilla\NativeMessagingHosts\org.hwcrypto.native" '' '$INSTDIR\org.hwcrypto.native.firefox.json'
WriteRegStr HKCU "SOFTWARE\Google\Chrome\NativeMessagingHosts\org.hwcrypto.native" '' '$INSTDIR\org.hwcrypto.native.json'

; By default start on login
WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Run" 'Web-eID' '$INSTDIR\Web-eID.exe'

; Make shortcut on desktop
CreateShortCut "$DESKTOP\Web eID.lnk" "$INSTDIR\Web-eID.exe"
CreateDirectory "$SMPROGRAMS\Web eID"
CreateShortCut "$SMPROGRAMS\Web eID\Start Web eID.lnk" "$INSTDIR\Web-eID.exe"
CreateShortCut "$SMPROGRAMS\Web eID\Uninstall Web eID.lnk" "$INSTDIR\uninstall.exe"

writeUninstaller "$INSTDIR\uninstall.exe"

ExecShell "open" "$INSTDIR\Web-eID.exe"
Sleep 1000
ExecShell "open" "https://web-eid.com/?installer=windows-local&version=${VERSION}"

SectionEnd


Section "uninstall"
SetAutoClose true
MessageBox MB_OK "Make sure Web eID app is closed before continuing"
rmDir /r "$LOCALAPPDATA\Web eID"
rmDir /r "$SMPROGRAMS\Web eID"
Delete "$DESKTOP\Web eID.lnk"
SetRegView 32
DeleteRegKey HKCU "SOFTWARE\Mozilla\NativeMessagingHosts\org.hwcrypto.native"
DeleteRegKey HKCU "SOFTWARE\Google\Chrome\NativeMessagingHosts\org.hwcrypto.native"
SetRegView 64
DeleteRegKey HKCU "SOFTWARE\Mozilla\NativeMessagingHosts\org.hwcrypto.native"
DeleteRegKey HKCU "SOFTWARE\Google\Chrome\NativeMessagingHosts\org.hwcrypto.native"
SectionEnd
