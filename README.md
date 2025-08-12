This project will be firmware downloader for some old mp3 players and FM
transmitters based on Actions Semiconductor family of chips like
AK-2XXX / ATJ-2XXX / ATS-2XXX.

For now, application has basic set of commands, allowing geting information
about device and firmware, and manual firmware read - you must choose on your
own starting sector and length of dump (and other optional control parameters).

It should work for following vendor:product device pairs:
10D6 : 1100  MPMan MP-Ki 128 MP3 Player/Recorder
10D6 : 1101  D-Wave 2GB MP4 Player / AK1025 MP3/MP4 Player
10D6 : 8888  ADFU Device
10D6 : ff51  ADFU Device
10D6 : ff61  MP4 Player
and possibly others form vendor "10D6 Actions Semiconductor Co., Ltd".