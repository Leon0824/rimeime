@echo off
if exist env.bat call env.bat
set OLD_PATH=%PATH%
if defined DEV_PATH set PATH=%DEV_PATH%;%OLD_PATH%
rem %comspec%
%comspec% /k ""C:\Program Files\Microsoft Visual Studio 9.0\VC\vcvarsall.bat"" x86
