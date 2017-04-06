REM Bring in vstudio tools if not run from developer prompt
WHERE /q nmake.exe
IF %ERRORLEVEL% NEQ 0 (
  ECHO "No Visual Studio environment found, loading VS 2015"
  CALL "C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\Tools\VsDevCmd.bat"
)
REM For some reason running of lrelease before qmake is needed on Windows
c:\Qt\5.8\msvc2015\bin\lrelease.exe web-eid.pro || exit /b
c:\Qt\5.8\msvc2015\bin\qmake.exe -config release || exit /b
nmake %* || exit /b
REM copy the interesting files for testing purposes
REM (installer picks them up from source)
c:\Qt\5.8\msvc2015\bin\windeployqt release\web-eid.exe
copy c:\OpenSSL-Win32\bin\libeay32.dll release\
copy c:\OpenSSL-Win32\bin\ssleay32.dll release\
