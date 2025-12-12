@echo off
if not exist build mkdir build
pushd build
cl /EHsc -FC -diagnostics:column -Fetinyrenderer -WL -nologo -Zi -O2 ..\main.cpp  /link -opt:ref -incremental:no
popd