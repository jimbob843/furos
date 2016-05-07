@echo off
set FUROSROOT=C:\Projects\Furos
set TOOLSROOT=%FUROSROOT%\Tools
set PATH=%PATH%;%TOOLSROOT%\NASM
set PATH=%PATH%;%TOOLSROOT%\DJGPP\bin
set PATH=%PATH%;%TOOLSROOT%\UnixUtils\usr\local\wbin
set PATH=%PATH%;%TOOLSROOT%\BFI
set DJGPP=%TOOLSROOT%\DJGPP\djgpp.env

echo Furos build environment configured.
echo.


