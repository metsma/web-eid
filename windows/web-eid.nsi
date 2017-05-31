!include "MUI.nsh"
!define MUI_ICON "artwork\win_icon.ico"
SetCompressor /solid /FINAL lzma
!insertmacro MUI_PAGE_INSTFILES
Name "Web eID per-user installer"
OutFile "Web-eID_${VERSION}.exe"
ShowInstDetails nevershow
SilentInstall normal
RequestExecutionLevel user

InstallDir "$LOCALAPPDATA\Web eID"


Section
MessageBox MB_OK "Please be aware: this is an evaluation release and will expire on 11. July 2017!"
SetAutoClose true
SetOutPath "$INSTDIR"
AllowSkipFiles off
IfFileExists $INSTDIR\Web-eID.exe 0 +5
MessageBox MB_OK "As you are upgrading, we need to quit the current Web eID app before installing the new one.$\nYou will have to re-load open browser sessions that are using it."
DetailPrint "Stopping Web eID app ..."
nsExec::Exec '"$INSTDIR\web-eid-bridge.exe" --quit'
Sleep 5000

File src\release\Web-eID.exe
File src\nm-bridge\release\web-eid-bridge.exe

File windows\org.hwcrypto.native.json
File windows\org.hwcrypto.native.firefox.json

File C:\Qt\5.8\msvc2015\bin\Qt5Core.dll
File C:\Qt\5.8\msvc2015\bin\Qt5Gui.dll
File C:\Qt\5.8\msvc2015\bin\Qt5Network.dll
File C:\Qt\5.8\msvc2015\bin\Qt5PrintSupport.dll
File C:\Qt\5.8\msvc2015\bin\Qt5Svg.dll
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

SetOutPath "$INSTDIR\iconengines"
File C:\Qt\5.8\msvc2015\plugins\iconengines\qsvgicon.dll

; Make shortcut on desktop
CreateShortCut "$DESKTOP\Web eID.lnk" "$INSTDIR\Web-eID.exe"
CreateDirectory "$SMPROGRAMS\Web eID"
CreateShortCut "$SMPROGRAMS\Web eID\Start Web eID.lnk" "$INSTDIR\Web-eID.exe"
CreateShortCut "$SMPROGRAMS\Web eID\Uninstall Web eID.lnk" "$INSTDIR\uninstall.exe"

writeUninstaller "$INSTDIR\uninstall.exe"

ExecShell "open" "$INSTDIR\Web-eID.exe"
DetailPrint "Starting Web eID app"
SectionEnd

Section "uninstall"
SetAutoClose true
DetailPrint "Stopping Web eID app ..."
nsExec::Exec '"$INSTDIR\web-eid-bridge.exe" --quit'
Sleep 5000
rmDir /r "$LOCALAPPDATA\Web eID"
rmDir /r "$SMPROGRAMS\Web eID"
Delete "$DESKTOP\Web eID.lnk"
SectionEnd
