This project will be firmware downloader for some old mp3 players and FM
transmitters based on Actions Semiconductor family of chips like
AK-2XXX / ATJ-2XXX / ATS-2XXX.

For now, for device it found, dump first 32 MB of firmware logical sectors to
fw_log.bin, first 32MB of phisical sectors to fw_phy.bin and RAM to fw_ram.bin.
RAM dump may be a garbage since you device may not have full RAM access
functionality.
