@echo off
setlocal enabledelayedexpansion

if not exist build mkdir build
pushd build
del *.pdb > NUL 2> NUL

echo [Compiling with msvc]
echo Compiling game
echo PDB > game-lock.temp
call cl -FC -nologo -WX -Od -Z7 -LDd ..\src\game.c /link -incremental:no -opt:ref /EXPORT:update_and_render
del game-lock.temp

echo Compiling engine
call cl -FC -nologo -WX -Od -Z7 ..\src\main.c /link -incremental:no -opt:ref user32.lib gdi32.lib
popd build