WHERE /q nmake.exe
IF %ERRORLEVEL% NEQ 0 (
  ECHO "No Visual Studio environment found, loading VS 2015"
  CALL "C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\Tools\VsDevCmd.bat"
)
c:\Qt\5.8\msvc2015\bin\qmake.exe -config release
nmake %*
c:\Qt\5.8\msvc2015\bin\windeployqt release\hwcrypto-native.exe
copy c:\OpenSSL-Win32\bin\libeay32.dll release\
copy c:\OpenSSL-Win32\bin\ssleay32.dll release\


