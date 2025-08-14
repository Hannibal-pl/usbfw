#include <stdio.h>
#include <string.h>

#include "usbfw.h"

// keep header copy in memory; make sure it is always connected with currently open file
FW_AFI_HEADER afi_header;

uint32_t afi_offset(void) {
	uint32_t offset = sizeof(FW_AFI_HEADER);

	for (uint32_t i = 0; i < 126; i++) {
		if (afi_header.diritem[i].filename[0]) {
			offset = afi_header.diritem[i].offset + afi_header.diritem[i].length;
		}
	}

	return offset;
}


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

// make afi file from provided data
void afi_add_whole(FILE *fafi, FW_AFI_DIR_ENTRY *afi_entry, uint8_t* data) {
	// fill gaps
	afi_entry->offset = afi_offset();
	afi_entry->checksum = checksum32((uint32_t *)data, afi_entry->length, true);

	// write data
	fseek(fafi, afi_entry->offset, SEEK_SET);
	fwrite(data, afi_entry->length, 1, fafi);


	// update header
	for (uint32_t i = 0; i < 126; i++) {
		if (!afi_header.diritem[i].filename[0]) {
			memcpy(&afi_header.diritem[i], afi_entry, sizeof(FW_AFI_DIR_ENTRY));
			break;
		}
	}
	afi_header.checksum = checksum32((uint32_t *)&afi_header, sizeof(FW_AFI_HEADER) - sizeof(uint32_t), true);
	fseek(fafi, 0, SEEK_SET);
	fwrite(&afi_header, sizeof(FW_AFI_HEADER), 1, fafi);
}

// make afi file from data already appended to container
void afi_add_appended(FILE *fafi, FW_AFI_DIR_ENTRY *afi_entry) {
	// fill gaps
	afi_entry->offset = afi_offset();

	// update header
	for (uint32_t i = 0; i < 126; i++) {
		if (!afi_header.diritem[i].filename[0]) {
			memcpy(&afi_header.diritem[i], afi_entry, sizeof(FW_AFI_DIR_ENTRY));
			break;
		}
	}
	afi_header.checksum = checksum32((uint32_t *)&afi_header, sizeof(FW_AFI_HEADER) - sizeof(uint32_t), true);
	fseek(fafi, 0, SEEK_SET);
	fwrite(&afi_header, sizeof(FW_AFI_HEADER), 1, fafi);
}
