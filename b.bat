@echo off
setlocal enabledelayedexpansion

if not exist build mkdir build
pushd build
del *.pdb > NUL 2> NUL

set test=
set compiler=cl
set mode=debug
set game_link_flags=/link -incremental:no -opt:ref /EXPORT:update_and_render libfreetype-r.lib
set engine_link_flags=/link -incremental:no -opt:ref user32.lib gdi32.lib

if "%mode%" == "debug"          set compiler_flags=-FC -nologo -WX -Od -Zi
if "%mode%" == "release-debug"  set compiler_flags=-FC -nologo -WX -O2 -Zi
if "%mode%" == "release"        set compiler_flags=-FC -nologo -WX -O2

echo [Compiler %compiler% - %mode%]
if defined test (
    echo Compiling test suite
    echo PDB > test-lock.temp
    call cl %compiler_flags% -I..\thirdparty\freetype-2.13.3\custom -I..\thirdparty\freetype-2.13.3\include2 -LD ..\src\test.c %game_link_flags%
    del test-lock.temp
) else (
    echo Compiling game
    echo PDB > game-lock.temp
    call cl %compiler_flags% -I..\thirdparty\freetype-2.13.3\custom -I..\thirdparty\freetype-2.13.3\include2 -LD ..\src\game.c %game_link_flags%
    del game-lock.temp
)

echo Compiling engine
if defined test (
    call cl -DTEST_SUITE %compiler_flags% ..\src\main.c %engine_link_flags%
) else (
    call cl %compiler_flags% ..\src\main.c %engine_link_flags%
)
popd build
