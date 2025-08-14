This project is a firmware downloader for some old mp3 players and FM
transmitters based on Actions Semiconductor family of chips like
AK-2XXX / ATJ-2XXX / ATS-2XXX.


For now, application has ability to:

 -- Use standard USB mass storage SCSI/SFF-8070 features like:

    - display device info (INQUIRY);

    - display capacity (CAPACITY);

    - read/write sector (READ10/WRITE10).

   Those shoul work also for any SCSI/SFF-8070 compatible devices.


 -- Enumerate USB devices and display Actions compatible ones.

 -- Enter Actions firmware update mode and:

    - display firmware header information including listing of firmware files;

    - display sysinfo;

    - dump firmware in RAW and AFI (s1fxw compatible) mode;

    - read any selected sector from firmware physical/logical area;

    - read device RAM (if device has this feature);

    - perform firmware ENTRY command - DANGEROUS;



It should work for following vendor:product device pairs:

 -- 10D6:1100  MPMan MP-Ki 128 MP3 Player/Recorder

 -- 10D6:1101  D-Wave 2GB MP4 Player / AK1025 MP3/MP4 Player

 -- 10D6:8888  ADFU Device

 -- 10D6:FF51  ADFU Device

 -- 10D6:FF61  MP4 Player

 -- and possibly others form vendor 10D6 - Actions Semiconductor Co., Ltd".

