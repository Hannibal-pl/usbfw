#include <string.h>

#include "usbfw.h"

bool init_act(USB_BULK_CONTEXT *uctx) {
	CBW cbw;

	// check for Actions device
	ACTIONSUSBD actid;
	command_init_act_identify(&cbw, 1);
	if (command_perform_act_identify(&cbw, uctx, &actid)) {
		printf("Error: Cannot identify Actions device.\n");
		return false;
	} else if (strncmp(actid.actionsusbd, "ACTIONSUSBD", 11) != 0) {
		printf("Error: Actions indentifier not match\n");
		return false;
	}

	// init firmware mode
	uint8_t resp;
	command_init_act_init(&cbw);
	if ((command_perform_act_init(&cbw, uctx, &resp)) || (resp != 0xFF)) {
		printf("Error: Unable to init firmware mode\n");
		return false;
	}

	return true;
}

uint32_t search_alternate_fw(USB_BULK_CONTEXT *uctx, uint8_t lun, uint32_t max_lba) {
	FW_HEADER fw_header;
	CBW cbw;

	printf("Searching for alternate header...  ");
	for (uint32_t i = 8; i < max_lba; i++) {
		command_init_act_readone(&cbw, lun, i, true);
		if (command_perform_act_readone(&cbw, uctx, (uint8_t *)&fw_header)) {
			printf("\nError: Searching alternate header failed at sector %i\n", i);
			return 0xFFFFFFFF;
		}
		// header found
		if (fw_header.magic == 0x0FF0AA55) {
			printf("\bfound at sector 0x%08X\n\n", i);
			return i;
		}

		if ((i & 0xF) == 0) {
			display_spinner();
		}
	}

	printf("\bnot found.\n\n");
	return 0;
}

bool get_fw_header(USB_BULK_CONTEXT *uctx, FW_HEADER *fw_header, uint8_t lun, uint32_t start_lba) {
	CBW cbw;

	for (uint32_t i = 0; i < 16; i++) {
		command_init_act_readone(&cbw, app.lun, start_lba + i, true);
		if (command_perform_act_readone(&cbw, uctx, ((uint8_t *)fw_header) + (i * SECTOR_SIZE))) {
			printf("Error: Reading header failed at sector %i\n", i);
			return false;
		}
	}

	if (fw_header->magic != 0x0FF0AA55) {
		printf("Error: Readed data is isn't proper actions firmware header.\n");
		return false;
	}

	return true;
}

uint32_t get_fw_size(USB_BULK_CONTEXT *uctx, uint8_t lun, uint32_t start_lba) {
	FW_HEADER fw_header;
	uint32_t file_start = 0;
	uint32_t file_len = 0;

	if (!get_fw_header(uctx, &fw_header, app.lun, start_lba)) {
		return 0;
	}

	// find position of last file in firmware ...
	for (uint32_t i = 0; i < 240; i++) {
		FW_DIR_ENTRY *entry = &fw_header.diritem[i];
		if (entry->filename[0] != 0) {
			if (entry->offset > file_start) {
				file_start = entry->offset;
				file_len = entry->length;
			}
		}
	}

	// .. and add it length to get firmware size;
	return file_start + ((file_len + SECTOR_SIZE - 1) / SECTOR_SIZE);
}
