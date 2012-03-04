#!/bin/sh
cd `dirname $0`
make clean
if test -z "$1"; then
    #for win32
    make XCFLAGS="-march=i686 -mfpmath=sse -msse -fexcess-precision=fast" XLDFLAGS=-Wl,--large-address-aware
    #for win64
    make CROSS=x86_64-w64-mingw32- SUFFIX=64 XCFLAGS=-fexcess-precision=fast
else
    #debug builds
    make debug
    make debug CROSS=x86_64-w64-mingw32- SUFFIX=64
fi
