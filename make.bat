WHERE /q nmake.exe
IF %ERRORLEVEL% NEQ 0 (
  ECHO "No Visual Studio environment found, loading VS 2015"
  CALL "C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\Tools\VsDevCmd.bat"
)
nmake %1
