avs2pipemod is a modified version of avs2pipe basically only for my purpose.
Thanks to Chris Beswick who releases avs2pipe under GPL.

source code : https://github.com/chikuzen/avs2pipemod

Differences with original
* Display total elapsed time.
* contents of 'info'
* New option 'benchmark'.
* New option 'trim'.
* New option 'dumptxt'
* New option 'filters'
* New option 'dumpprops'
* 'y4mp', 'y4mt', 'y4mb' and 'rawvideo' options instead of 'video'.
* FieldBased input will be corrected to framebased on yuv4mpeg2 output modes.
* Colorspace conversion that takes colormatrix and interlace into consideration.
* 'wav', 'extwav' and 'rawaudio' option instead of 'audio'.
* Convert bit depth function in audio output mode.


Usage
	Execute avs2pipemod.exe without any options.
	Then help will be displayed.

Requirement
	- Avisynth2.6.0final / Avisynth+ 3.7.5 or later.
	- Windows 7 sp1 or later.
	- Microsoft Visual C++ Redistributable v14.


                                        written by Oka Motofumi