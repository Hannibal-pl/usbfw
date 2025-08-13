#include <stdio.h>
#include <string.h>

#include "usbfw.h"

// keep header copy in memory; make sure it is always connected with currently open file
FW_AFI_HEADER afi_header;


FILE * afi_new_file(char *filename) {
	FILE *fafi = NULL;

	fafi = fopen(filename, "w");
	if (!fafi) {
		return NULL;
	}

	// create and write to file afi header
	memset(&afi_header, 0 , sizeof(FW_AFI_HEADER));
	memcpy(&afi_header.magic, "AFI", 3);
	afi_header.vendorId = app.vid;
	afi_header.productId = app.pid;
	afi_header.checksum = checksum32((uint32_t *)&afi_header, sizeof(FW_AFI_HEADER) - sizeof(uint32_t), true);

	fwrite(&afi_header, sizeof(FW_AFI_HEADER), 1, fafi);

	return fafi;
}
