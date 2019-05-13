@echo off
mkdir build 
pushd build
mkdir win64
pushd win64

cmake -G "Visual Studio 15 2017 Win64" ../../

popd
popd

cmake --build build/win64 --config Release