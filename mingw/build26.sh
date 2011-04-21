#!/bin/bash

cd `pwd`

pushd ../src

gcc -O2 -Wall -DA2P_AVS26 avs2pipe.c common.c wave.c avisynth25/avisynth.lib -o ../avs2pipe26_gcc.exe
strip ../avs2pipe26_gcc.exe

popd
