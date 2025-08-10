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
	{NULL, 0, NULL, 0}};


void usage(char *binfile) {
	printf("Usage: %s COMMAND [OPTIONS]\n", binfile);
}

void parseparams(int argc, char *argv[]) {
	int opt;

	while (true) {
		opt = getopt_long(argc, argv, "f:ed:l:L:c:iCIrRoph?", longopt, NULL);
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
			case 'H':
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
					printf("Error: You provide must device id.\n\n");
					exit(-1);
				}
				if (!parse_devid(optarg)) {
					printf("Error: Incorrect device id.\n\n");
					exit(-1);
				}
				break;
			case 'L':
				if (optarg == NULL) {
					printf("Error: You provide LUN number.\n\n");
					exit(-1);
				}
				uint32_t lun = strtoul(optarg, NULL, 0);
				if (lun > 7) {
					printf("Error: Incorrect LUN number.\n\n");
					exit(-1);
				}
				app.lun = (uint8_t)(lun & 7);
				break;
		}
	}


help:
	usage(basename(argv[0]));
	exit(0);
}
