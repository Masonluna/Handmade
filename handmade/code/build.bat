@echo off

mkdir Handmade\build
pushd Handmade\build

pwd
cl -FC -Zi ..\code\win32_handmade.cpp user32.lib Gdi32.lib
popd
