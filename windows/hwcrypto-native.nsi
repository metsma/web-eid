Name "Web eID per-user installer"
OutFile "Web-eID_${VERSION}-local.exe"
ShowInstDetails show
RequestExecutionLevel user

InstallDir "$LOCALAPPDATA\Web eID"


Section
SetOutPath "$INSTDIR"
AllowSkipFiles off

File host-qt\release\hwcrypto-native.exe

File windows\org.hwcrypto.native.json
File windows\org.hwcrypto.native.firefox.json

File C:\Qt\5.8\msvc2015\bin\Qt5Core.dll
File C:\Qt\5.8\msvc2015\bin\Qt5Gui.dll
File C:\Qt\5.8\msvc2015\bin\Qt5Network.dll
File C:\Qt\5.8\msvc2015\bin\Qt5PrintSupport.dll
File C:\Qt\5.8\msvc2015\bin\Qt5Widgets.dll
File C:\Qt\5.8\msvc2015\bin\Qt5WinExtras.dll
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


writeUninstaller "$INSTDIR\uninstall.exe"
ExecShell "open" "https://web-eid.com/?installer=windows-local&version=${VERSION}"

SectionEnd

Section "uninstall"
rmDir /r "$LOCALAPPDATA\Web eID"
SetRegView 32
DeleteRegKey HKCU "SOFTWARE\Mozilla\NativeMessagingHosts\org.hwcrypto.native"
DeleteRegKey HKCU "SOFTWARE\Google\Chrome\NativeMessagingHosts\org.hwcrypto.native"
SetRegView 64
DeleteRegKey HKCU "SOFTWARE\Mozilla\NativeMessagingHosts\org.hwcrypto.native"
DeleteRegKey HKCU "SOFTWARE\Google\Chrome\NativeMessagingHosts\org.hwcrypto.native"
SectionEnd