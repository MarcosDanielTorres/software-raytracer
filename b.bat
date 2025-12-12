@echo off
if not exist build mkdir build
pushd build
del *.pdb
echo [Compiling with msvc]
echo Compiling game
call cl -FC -nologo -WX -Od -Z7 -LDd ..\src\game.c /link -incremental:no -opt:ref /EXPORT:game_update_and_render
echo Compiling engine
call cl -FC -nologo -WX -Od -Z7 ..\src\win32_main.c /link -incremental:no -opt:ref user32.lib gdi32.lib
popd build