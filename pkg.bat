rem @echo off
if not exist pkg mkdir pkg

set RELEASE=%1
if "%RELEASE%" == "" set RELEASE=Debug

copy %RELEASE%\tlhsetup2.exe pkg
copy %RELEASE%\tlhsetup2.pdb pkg
copy %RELEASE%\TaglibHandler.dll pkg
copy %RELEASE%\TaglibHandler.pdb pkg
if not exist pkg\x64 mkdir pkg\x64
copy x64\%RELEASE%\TaglibHandler.dll pkg\x64
copy x64\%RELEASE%\TaglibHandler.pdb pkg\x64

copy README.txt pkg\README.txt

for /F "usebackq tokens=2* delims= " %%f in (`reg query "HKLM\SOFTWARE\Wow6432Node\Microsoft\Windows\CurrentVersion\App Paths\WinRAR.exe" /v Path`) do set RARPATH="%%g\rar.exe"
for /F "usebackq tokens=2* delims= " %%f in (`reg query "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\WinRAR.exe" /v Path`) do set RARPATH="%%g\rar.exe"
for /F "usebackq tokens=2* delims= " %%f in (`reg query "HKLM\SOFTWARE\7-Zip" /v Path`) do set ZPATH="%%g\7z.exe"

set VERSION=unknown
for /F "usebackq" %%f in (`svnversion`) do set VERSION="r%%f"

set OUTNAME=..\taglibhandler-%VERSION%

cd pkg
if exist %RARPATH% %RARPATH% a -av -r -m5 -s %OUTNAME% *
if exist %ZPATH% %ZPATH% a -mx9 -mson -sfx7z.sfx -r %OUTNAME% *
cd ..
