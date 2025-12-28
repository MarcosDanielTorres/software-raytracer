@echo off
setlocal enabledelayedexpansion

if not exist build mkdir build
pushd build
del *.pdb > NUL 2> NUL

echo [Compiling with msvc]
echo Compiling repetition tester
set optimization=-Od
call cl -FC -nologo -WX %optimization% -Zi -I..\src ..\tester\repetition_tester.c /link -incremental:no -opt:ref
popd build