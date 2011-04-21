@ECHO OFF

SET PATH=C:\MinGW\bin

pushd ..\src

ECHO Running GCC
gcc -O2 -Wall -DA2P_AVS26 avs2pipe.c common.c wave.c avisynth26\avisynth.lib -o ..\avs2pipe26_gcc.exe
strip ..\avs2pipe26_gcc.exe

popd
