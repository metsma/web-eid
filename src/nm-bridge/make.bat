REM Bring in vstudio tools if not run from developer prompt
WHERE /q nmake.exe
IF %ERRORLEVEL% NEQ 0 (
  ECHO "No Visual Studio environment found, loading VS 2015"
  CALL "C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\Tools\VsDevCmd.bat"
)
c:\Qt\5.8\msvc2015\bin\qmake.exe -config debug || exit /b
nmake %* || exit /b
