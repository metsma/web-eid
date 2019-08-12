@echo off
mkdir build 
pushd build
mkdir win64
pushd win64

cmake -G "Visual Studio 15 2017 Win64" ../../
cmake --build . --config Release
cpack -C Release -G WIX

set /p code_signing_password= "Enter the code signing password: "

signtool sign /v /f "Z:\Downloads\code_signing.p12" /p "%code_signing_password%" /fd SHA256 /tr http://timestamp.comodoca.com/?td=sha256 /td sha256 web-eid-*-win64.msi

popd
popd

