REM Bring in vstudio tools if not run from developer prompt
WHERE /q nmake.exe
IF %ERRORLEVEL% NEQ 0 (
  ECHO "No Visual Studio environment found, loading VS 2015"
  CALL "C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\Tools\VsDevCmd.bat"
)
REM For some reason running of lrelease before qmake is needed on Windows
c:\Qt\5.12.3\msvc2017\\bin\lrelease.exe src.pro || exit /b
c:\Qt\5.12.3\msvc2017\bin\qmake.exe -config debug || exit /b
nmake %* || exit /b
REM copy the interesting files for testing purposes
REM (installer picks them up from source)
copy nm-bridge\debug\web-eid-bridge.exe debug\
c:\Qt\5.12.3\msvc2017\bin\windeployqt debug\web-eid.exe
c:\Qt\5.12.3\msvc2017\bin\windeployqt debug\web-eid-bridge.exe
copy c:\OpenSSL-Win32\bin\libeay32.dll debug\
copy c:\OpenSSL-Win32\bin\ssleay32.dll debug\
