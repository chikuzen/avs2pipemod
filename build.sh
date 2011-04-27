#!/bin/bash

usage()
{
cat << _EOT_
    invalid arguments

    usage : build.sh [option]
      make    - compile binaries
      clean   - delete binaries and src/version.h
      package - compress binaries and .txt to zip (require 7z.exe)
_EOT_
exit
}

if [ $# -ne 1 ]; then
    usage
fi

cd `dirname $0`

case $1 in
    make )
        echo "#define A2PM_VERSION \"rev `git rev-list HEAD -n 1 | cut -c 1-7`\"" > src/version.h
        echo "#define A2PM_DATE_OF_BUILD \"`date -u +"%Y%m%d-%H:%M"`[UTC]\"" >> src/version.h
        cd src
        gcc -O3 -Wall -march=i686 -DA2P_AVS26 avs2pipe.c common.c wave.c avisynth26/avisynth.lib -o ../avs2pipe26mod.exe
        strip ../avs2pipe26mod.exe
        gcc -O3 -Wall -march=i686 avs2pipe.c common.c wave.c avisynth25/avisynth.lib -o ../avs2pipemod.exe
        strip ../avs2pipemod.exe
        ;;
    clean )
        rm ./avs2pipe*.exe
        rm src/version.h
        ;;
    package )
        7z a -tzip avs2pipemod-`date -u +"%Y%m%d"` ./avs2pipe*.exe ./*.txt
        ;;
    *)
        usage
        ;;
esac
