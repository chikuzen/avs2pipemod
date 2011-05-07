avs2pipemod is a modified version of avs2pipe basically only for me.

source code : https://github.com/chikuzen/avs2pipemod

Differences with original
* Display total elapsed time.
* contents of 'info'
* New options 'benchmark'.
* 'y4mp', 'y4mt', 'y4mb' and 'rawvideo' options instead of 'video'.
* FieldBased input will be corrected to framebased on yuv4mpeg2 output modes.
* Colorspace conversion that takes colormatrix and interlace into consideration.
* 'rawaudio(output audio without header)' option.
* Convert bit depth function in audio output mode.
* Only the compilation with mingw/msys is supported.

                                        written by Oka Motofumi