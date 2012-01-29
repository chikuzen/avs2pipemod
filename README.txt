avs2pipemod is a modified version of avs2pipe basically only for my purpose.
Thanks to Chris Beswick who releases avs2pipe under GPL.

source code : https://github.com/chikuzen/avs2pipemod

Differences with original
* Display total elapsed time.
* contents of 'info'
* New option 'benchmark'.
* New option 'trim'.
* New option 'x264raw'
* 'y4mp', 'y4mt', 'y4mb' and 'rawvideo' options instead of 'video'.
* FieldBased input will be corrected to framebased on yuv4mpeg2 output modes.
* Colorspace conversion that takes colormatrix and interlace into consideration.
* 'wav', 'extwav' and 'rawaudio' option instead of 'audio'.
* Convert bit depth function in audio output mode.
* Only the compilation with mingw/msys is supported.

                                        written by Oka Motofumi