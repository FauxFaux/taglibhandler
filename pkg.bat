@echo off
if not exist pkg mkdir pkg

copy Debug\TaglibHandler.dll pkg
copy Debug\TaglibHandler.pdb pkg
if not exist pkg\x64 mkdir pkg\x64
copy x64\Debug\TaglibHandler.dll pkg\x64
copy x64\Debug\TaglibHandler.pdb pkg\x64

copy README.txt pkg\README.txt