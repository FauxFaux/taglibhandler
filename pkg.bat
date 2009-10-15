rem @echo off
rd /q /s pkg
mkdir pkg

set RELEASE=%1
if "%RELEASE%" == "" set RELEASE=Debug

copy %RELEASE%\tlhsetup2.exe pkg
rem copy %RELEASE%\tlhsetup2.pdb pkg
copy %RELEASE%\TaglibHandler.dll pkg
rem copy %RELEASE%\TaglibHandler.pdb pkg
if not exist pkg\x64 mkdir pkg\x64
copy x64\%RELEASE%\TaglibHandler.dll pkg\x64
rem copy x64\%RELEASE%\TaglibHandler.pdb pkg\x64

if "%RELEASE%" == "Release" goto nopdb
copy %RELEASE%\tlhsetup2.pdb pkg
copy %RELEASE%\TaglibHandler.pdb pkg
copy x64\%RELEASE%\TaglibHandler.pdb pkg\x64
:nopdb

copy README.txt pkg\README.txt

for /F "usebackq tokens=2* delims= " %%f in (`reg query "HKLM\SOFTWARE\7-Zip" /v Path`) do set ZPATH="%%g\7z.exe"

set VERSION=unknown
for /F "usebackq" %%f in (`svnversion`) do set VERSION="r%%f"

set OUTNAME=..\taglibhandler-%VERSION%

cd pkg
if exist %ZPATH% %ZPATH% a -mx9 -ms=on -sfx7z.sfx -r %OUTNAME% *
cd ..
