@echo off
setlocal enabledelayedexpansion

if not exist build mkdir build
pushd build
del *.pdb > NUL 2> NUL

set compiler=cl
set mode=debug

if "%mode%" == "debug"   set compiler_flags=-Od -Zi
if "%mode%" == "release" set compiler_flags=-O2

echo [Compiler %compiler% - %mode%]
echo Compiling game
echo PDB > game-lock.temp
call cl -FC -nologo -WX %compiler_flags% -LD ..\src\game.c /link -incremental:no -opt:ref /EXPORT:update_and_render
del game-lock.temp

echo Compiling engine
call cl -FC -nologo -WX %compiler_flags% ..\src\main.c /link -incremental:no -opt:ref user32.lib gdi32.lib
popd build