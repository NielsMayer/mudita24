## Mudita24 - Control Panel for Ice1712 Audio Cards  ##

Mudita24 is a modification of the Linux alsa-tools' 

&lt;A href="http://git.alsa-project.org/?p=alsa-tools.git;a=tree;f=envy24control;hb=HEAD"&gt;

envy24control(1)

&lt;/A&gt;

: an application controlling the digital mixer, channel gains and other hardware settings for sound cards based on the 

&lt;A href="http://alsa.cybermirror.org/manuals/icensemble/envy24.pdf"&gt;

VIA Ice1712 chipset

&lt;/A&gt;

 aka 

&lt;A href="http://www.via.com.tw/en/products/audio/controllers/envy24/"&gt;

Envy24

&lt;/A&gt;

. Unlike most ALSA mixer controls, this application displays a level meter for each input and output channel and maintains peak level indicators.  This is based on Envy24's hardware peak metering feature.

Mudita24 provides  alternate name to avoid confusion with "envy24control 0.6.0" until changes in this version propagate upstream. As balance to the "Envy", this project needed some 

&lt;A href="http://en.wikipedia.org/wiki/Envy#In\_philosophy"&gt;

Mudita

&lt;/A&gt;

 "In Buddhism the third of the four divine abidings is mudita, taking joy in the good fortune of another. This virtue is considered the antidote to envy and the opposite of schadenfreude."

This utility is preferable to alsamixer(1) for those with ice1712-based cards: 

&lt;A href="http://www.m-audio.com/images/global/manuals/Delta1010LT-Manual.pdf"&gt;

M-Audio Delta 1010

&lt;/A&gt;

, 

&lt;A href="http://www.m-audio.com/images/global/manuals/Delta1010LT-Manual.pdf"&gt;

Delta 1010LT

&lt;/A&gt;

, Delta DiO 2496, 

&lt;A href="http://www.m-audio.com/images/global/manuals/070212\_Delta66\_UG\_EN01.pdf"&gt;

Delta 66

&lt;/A&gt;

, 

&lt;A href="http://www.m-audio.com/images/global/manuals/Delta44\_Manual.pdf"&gt;

Delta 44

&lt;/A&gt;

, Delta 410 and 

&lt;A href="http://www.m-audio.com/images/global/manuals/Audiophile2496\_Manual.pdf"&gt;

Audiophile 2496

&lt;/A&gt;

. 

&lt;A href="ftp://ftp.terratec.net/Audio/EWS/88MT/Manual/EWS88MT\_Manual\_GB.pdf"&gt;

Terratec EWS 88MT

&lt;/A&gt;

, 

&lt;A href="ftp://ftp.terratec.de/Audio/EWS/88D/Manual/EWS88D\_Manual\_GB.pdf"&gt;

EWS 88D

&lt;/A&gt;

, 

&lt;A href="ftp://ftp.terratec.net/Audio/EWX/2496/Manual/EWX2496\_Manual\_GB.pdf"&gt;

EWX 24/96

&lt;/A&gt;

, 

&lt;A href="ftp://ftp.terratec.net/Audio/DMX6fire2496/Manual/DMX6fire2496\_Manual\_GB.pdf"&gt;

DMX 6Fire

&lt;/A&gt;

, Phase 88. Hoontech Soundtrack DSP 24, Soundtrack DSP 24 Value, Soundtrack DSP 24 Media 7.1. 

&lt;A href="http://www.event1.com/Support/Manuals/EZ8\_MANUAL\_V1\_0.pdf"&gt;

Event Electronics EZ8

&lt;/A&gt;

.  Digigram VX442. 

&lt;A href="http://www.lionstracs.com/store/msaudio-board-envy24-p-122.html"&gt;

Lionstracs

&lt;/A&gt;

, 

&lt;A href="http://www.lionstracs.com/store/mixer-board-p-210.html"&gt;

Mediastaton

&lt;/A&gt;

. 

&lt;A href="http://www.musonik.com/index.php/terrasoniq-ts-88.html"&gt;

Terrasoniq TS 88

&lt;/A&gt;

. 

&lt;A href="http://www.soundonsound.com/sos/aug02/articles/edirolda2496.asp"&gt;

Roland/Edirol DA-2496

&lt;/A&gt;

.

Now available, version 1.XX:
  * subversion: ` svn checkout http://mudita24.googlecode.com/svn/trunk/mudita24 mudita24-svn `
  * Included with upcoming 

&lt;A href='http://www.remastersys.com/forums/index.php?topic=934.msg5335'&gt;

AV Linux 4.0

&lt;/A&gt;

.

Tim E. Real has made a patch available which allows the "Monitor Inputs" and "Monitor PCMs" metering and attenuators to resize correctly with a window resize, and resolves complaints regarding Gtk's "detent" on the sliders: http://lalists.stanford.edu/lad/2010/08/0059.html

'envy24control' is part of the "alsa-tools" package. For example in Fedora 12, it's part of alsa-tools-1.0.22-1.fc12. This "mudita24" package
updates/replaces the alsa-tools /usr/bin/envy24control application.
The default "./configure ; make ; sudo make install" process on the
source-code leaves a binary in /usr/local/bin/envy24control and places
the man-page in /usr/local/man/man1/envy24control.1 . This means you
can still use the standard alsa-tools version in
/usr/bin/envy24control .

Screenshots (controlling either Terratec DMX6Fire or M-Audio Delta 66):

  * New: Mudita24UbuntuStudio1010BetaTheme
  * New: [Mudita24 on AV Linux](http://www.remastersys.com/forums/index.php?topic=1208.0)

![http://nielsmayer.com/envy24control/Mudita24-102-Monitor-Inputs.png](http://nielsmayer.com/envy24control/Mudita24-102-Monitor-Inputs.png)
![http://nielsmayer.com/envy24control/Mudita24-102-Monitor-Outputs.png](http://nielsmayer.com/envy24control/Mudita24-102-Monitor-Outputs.png)
![http://nielsmayer.com/envy24control/Mudita24-102-Patchbay+Router.png](http://nielsmayer.com/envy24control/Mudita24-102-Patchbay+Router.png)
![http://nielsmayer.com/envy24control/Mudita24-102-Hardware-Settings.png](http://nielsmayer.com/envy24control/Mudita24-102-Hardware-Settings.png)
![http://nielsmayer.com/envy24control/Mudita24-102-Analog-Volume.png](http://nielsmayer.com/envy24control/Mudita24-102-Analog-Volume.png)
![http://nielsmayer.com/envy24control/Mudita24-102-About.png](http://nielsmayer.com/envy24control/Mudita24-102-About.png)

Changes since recent 1.0.0 and 1.0.1 releases:

  * Peak-meter display is in dBFS, corresponding to displayed dBFS peak-meter value and scale-widget dB labeling.

  * Hardware mixer input attenuators provide more precise control to the 0 to -48dB range of adjustment, turning the associated input "off" when the slider is moved to bottom of the scale. External MIDI control of the hardware mixer via --midichannel and --midienhanced options unaffected by this change.

  * For M-Audio Delta series, add display of "Delta IEC958 Input Status" under "Hardware Settings."

  * Command line options --no\_scale\_mark, --channel\_group\_modulus affect layout and presence of dB markings for sliders. --channel\_group\_modulus allows override of Left/Right grouping of dB labels for multichannel applications.

  * Control of peak-meter coloring via --lights\_color and --bg\_color options. Reasonable default colors used when these options are not set. (1.0.1's use of Gtk skin to provide an automatic color choice didn't work out that well on some systems.)

  * Fixed command-line options --card and --device to allow valid ALSA card and CTL device names  ( https://bugzilla.redhat.com/show_bug.cgi?id=602900 ).

  * Profiles created in ~/.envy24control and not "~/envy24control" ( http://bugtrack.alsa-project.org/alsa-bug/view.php?id=4738 ).

Summary of previous updates from envy24control 0.6.0 (GIT HEAD) to "1.0.3":

  * Implemented "Peak Hold" functionality in meters; reimplemented meters to do away with inefficient "faux LED" peak-meter display.

  * Significantly reduced the number of timer interrupts generated by this program by slowing down all updates to 10 per second - previously meters updated 25x/second!

  * All volumes are represented as decibels, including the 0 to -48dB range of the hardware peak-meters, the 0 -to- -48dB&off attenuation for all inputs to the digital mixer, the 0 -to- -63dB attenuation of the analog DAC, and the +18 -to- -63dB attenuation/amplification of the analog ADC.

  * All gtk "scale" widgets have dB legends; the "Page Up" "Page Down" keys allow rapid movement between the marked levels, and "Up Arrow" and "Down Arrow" allow fine-adjustment.