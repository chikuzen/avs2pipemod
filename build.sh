#!/bin/bash

cd `dirname $0`
pushd src
gcc -O3 -Wall -march=i686 -DA2P_AVS26 avs2pipe.c common.c wave.c avisynth25/avisynth.lib -o ../avs2pipemod26.exe
gcc -O3 -Wall -march=i686 avs2pipe.c common.c wave.c avisynth25/avisynth.lib -o ../avs2pipemod.exe
popd
strip ./avs2pipemod26.exe
strip ./avs2pipemod.exe
