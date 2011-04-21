avs2pipe is a tool to output y4m video, wav audio, dump some info about the
input avs clip or suggest x264 blu-ray encoding settings.

Usage: avs2pipe [audio|video|info|x264] input.avs
   audio  - output wav extensible format audio to stdout.
   video  - output yuv4mpeg2 format video to stdout.
   info   - output information about aviscript clip.
   x264bd - suggest x264 arguments for blu-ray disc encoding.


It simply takes a path to an avs script that returns a clip with audio and/or
video and outputs it as a y4m (--video) or wav (--audio) stream to stdout. It
can also generate information about the clip (--info) in an easily parsable
format for gui's.

The advantage of avs2pipe over other solutions, other than it doing multiple
things in one and supporting more colorspaces is that it links directly with
avisynth.dll via the C interface, so it compiles with both Visual Studio and 
MinGW and when used with wine in linux simply requires that the avisynth dll's 
are in PATH, so no need to have vfw configured correctly. Also as it uses 
WAVE_FORMAT_EXTENSIBLE for wav output and directly takes data from avisynth 
it will output exactly the same data as avisynth uses internally.


Source is provided in the src folder and is released under the GPL-3.0.
In addition to the source is:

src\avisynth25 - header and link lib from AviSynth 2.5.8
src\avisynth26 - header from http://git.videolan.org/?p=x264.git;a=blob;f=extras/avisynth_c.h;hb=HEAD 
                 link lib from AviSynth AVS 2.6.0 Alpha 2 [090927]


Projects and scripts to build the source are located in:

vs2010     - Project for Visual Studio 2010 Express
mingw      - Batch file MinGW on Windows, sh script for MinGW under Linux


The YUV4MPEG2 output is inspired by Avs2YUV by Loren Merritt, it is basically
the same method, converted from C++ to C. The changes are:

Interlaced Support - Picks up the flags for interlaced content from
                     AssumeFieldBased(), AssumeTFF() etc. and will pass this 
                     on to programs such as x264 eliminating the need to add
                     flags to the encoder command.

Extended Color Spaces - Thanks to Chikuzen when compiled against AviSynth 2.6
                        there is support for a whole host of extra colorspaces.


The WAV output is coded from scratch using specs from "the internet" and so
could be full of problems, altho I have not found any in testing yet.



Examples:

avs2pipe info input.avs
avs2pipe info input.avs > info.txt

avs2pipe video input.avs | x264 --stdin y4m - --output video.h264
avs2pipe audio input.avs | neroAacEnc -q 0.25 -if - -of audio.aac

avs2pipe audio input.avs > output.wav


Included Binaries:

avs2pipe_gcc.exe - compiled with MinGW gcc 4.5.2 on Windows
avs2pipe_vs.exe  - compiled with Visual Studio 2010 Express cl 16.00.30319.01

avs2pipe26_gcc.exe - gcc compiled for AviSynth 2.6 Alpha 2
avs2pipe26_vs.exe  - VS2010 compiled for AviSynth 2.6 Alpha 2


Download:

http://doobrymedia.com/avs2pipe-0.0.3.zip


Changes:

Version 0.0.3 - Added Blu-Ray x264 command generator based on specs from
                  http://sites.google.com/site/x264bluray/
                Added Support for new Colorspaces in AviSynth 2.6 with code
                  written by Chikuzen
Version 0.0.2 - Added Support for Interlaced Output, AssumeFieldBased etc.
Version 0.0.1 - Removed Progress Output for "Speed"
Pre-Alpha - Initial Release on doom10.org


Forum Links:

Original - http://doom10.org/index.php?topic=759
Doom9 Continuation - http://forum.doom9.org/showthread.php?t=160383

