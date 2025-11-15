@echo off
if not exist build mkdir build
pushd build
echo [Compiling with msvc]
call cl -nologo -WX -Od -Z7 ..\src\win32_main.c /link user32.lib gdi32.lib
popd build