@ECHO OFF

SET PATH=C:\MinGW\bin

pushd ..\src

ECHO Running GCC
gcc -O2 -Wall avs2pipe.c common.c wave.c avisynth25\avisynth.lib -o ..\avs2pipe_gcc.exe
strip ..\avs2pipe_gcc.exe

popd
