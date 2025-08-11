#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>

#include "usbfw.h"


const struct option longopt[] = {
	// commands
	{"enumerate-devices", 0, NULL, 'e'},
	{"inqiry", 0, NULL, 'i'},
	{"capacity", 0, NULL, 'C'},
	{"header-info", 0, NULL, 'I'},
	{"read", 0, NULL, 'r'},
	{"read-fw", 0, NULL, 'R'},
	{"help", 0, NULL, 'h'},
//	{"dump-raw-fw", 0, NULL, 'D'},
//	{"dump-afi-fw", 0, NULL, 'a'},

	// options
	{"file", 1, NULL, 'f'},
	{"device", 1, NULL, 'd'},
	{"lun", 1, NULL, 'L'},
	{"lba", 1, NULL, 'l'},
	{"block-count", 1, NULL, 'c'},
	{"logical", 0, NULL, 'o'},
	{"physical", 0, NULL, 'p'},
	{"show-dir", 0, NULL, 's'},
	{"detach", 0, NULL, 'D'},
	{"alternate", 0, NULL, 'a'},
	{NULL, 0, NULL, 0}};


void usage(char *binfile) {
	printf("Usage: %s COMMAND [OPTIONS]\n\n", binfile);
	printf("You must select one of following COMMANDS:\n");
	printf("  -e    --enumerate-devices    Enumerate all USB devices, and print all\n\
                               supported ones.\n");
	printf("  -i    --inquiry              Send INQUIRY SCSI command to the device and\n\
                               print information about it sendig back.\n");
	printf("  -C    --capacity             Send CAPACPITY SCSI command to the device and\n\
                               print device LBA count and sector size.\n");
	printf("  -I    --header-info          Fetches firmware header and prints all\n\
                               informations from it.\n");
	printf("  -r    --read                 Read data from selected logical device starting at\n\
                               provided start sector and sectors count.\n");
	printf("  -R    --read-fw              Read data from firmware area, either logical or\n\
                               phisical, starting at provided start sector and\n\
                               sectors count.\n");
	printf("  -h    --help                 Displays this help\n\n");
	printf("Additional you can use some of following OPTIONS.\n\n");
	printf("  -f    --file FILENAME        File name to where data read form device is saved.\n\
                               Default is \"%s\".\n", DEFAULT_OUT_FILENAME);
	printf("  -d    --device DEV           USB device to use be program in format VVVV:PPPP where\n\
                               VVVV is vendorID and PPPP is productID in hex. It is\n\
                               required by almost all commands.\n");
	printf("  -L    --lun LUN              Logical disk number on device used by command.\n\
                               Default is 0.\n");
	printf("  -l    --lba LBA              Starting sector of data transfer. Default is 0.\n");
	printf("  -c    --block-count COUNT    Number of sectors to transfer. Default is 1.\n");
	printf("  -o    --logical              In case of firmware area operations choose logical one.\n\
                               DEFAULT\n");
	printf("  -p    --phisical             In case of firmware area operations choose phisical one.\n");
	printf("  -s    --show-dir             Include contents of directory when display header info.\n");
	printf("  -D    --detach               Detach (restart) device at the end of execution frimware\n\
                               specific commands.\n");
	printf("  -a    --alternate            User alternate firmware (if present) in operations.\n");
}

void parseparams(int argc, char *argv[]) {
	int opt;

	while (true) {
		opt = getopt_long(argc, argv, "f:ed:l:L:c:iCIrRopsDah?", longopt, NULL);
		if (opt == -1) {
			break;
		}

		switch (opt) {
			// commands
			case 'e':
				if (app.cmd != APPCMD_NONE) {
					printf("Error: You have already select command.\n\n");
					goto help;
				}
				app.cmd = APPCMD_ENUMERATE;
				break;
			case 'i':
				if (app.cmd != APPCMD_NONE) {
					printf("Error: You have already select command.\n\n");
					goto help;
				}
				app.cmd = APPCMD_INQUIRY;
				break;
			case 'C':
				if (app.cmd != APPCMD_NONE) {
					printf("Error: You have already select command.\n\n");
					goto help;
				}
				app.cmd = APPCMD_CAPACITY;
				break;
			case 'I':
				if (app.cmd != APPCMD_NONE) {
					printf("Error: You have already select command.\n\n");
					goto help;
				}
				app.cmd = APPCMD_HEADINFO;
				break;
			case 'r':
				if (app.cmd != APPCMD_NONE) {
					printf("Error: You have already select command.\n\n");
					goto help;
				}
				app.cmd = APPCMD_READ;
				break;
			case 'R':
				if (app.cmd != APPCMD_NONE) {
					printf("Error: You have already select command.\n\n");
					goto help;
				}
				app.cmd = APPCMD_READ_FW;
				break;
			case '?':
			case 'h':
				goto help;
			// options
			case 'f':
				if (optarg == NULL) {
					printf("Error: You must provide output filename.\n\n");
					exit(-1);
				}
				app.filename = optarg;
				break;
			case 'd':
				if (optarg == NULL) {
					printf("Error: You must provide must device id.\n\n");
					exit(-1);
				}
				if (!parse_devid(optarg)) {
					printf("Error: Incorrect device id.\n\n");
					exit(-1);
				}
				break;
			case 'L':
				if (optarg == NULL) {
					printf("Error: You must provide LUN number.\n\n");
					exit(-1);
				}
				uint32_t lun = strtoul(optarg, NULL, 0);
				if (lun > 7) {
					printf("Error: Incorrect LUN number.\n\n");
					exit(-1);
				}
				app.lun = (uint8_t)(lun & 7);
				break;
			case 'l':
				if (optarg == NULL) {
					printf("Error: You must provide start LBA sector.\n\n");
					exit(-1);
				}
				app.lba = strtoul(optarg, NULL, 0);
				break;
			case 'c':
				if (optarg == NULL) {
					printf("Error: You must provide block count.\n\n");
					exit(-1);
				}
				app.bc = strtoul(optarg, NULL, 0);
				break;
			case 'o':
				app.is_logical = true;
				break;
			case 'p':
				app.is_logical = false;
				break;
			case 's':
				app.is_showdir = true;
				break;
			case 'D':
				app.is_detach = true;
				break;
			case 'a':
				app.is_alt_fw = true;
				break;
		}
	}

	if (app.cmd == APPCMD_NONE) {
		printf("Error: You must select a command.\n\n");
		goto help;
	}

	return;

help:
	usage(basename(argv[0]));
	exit(0);
}
