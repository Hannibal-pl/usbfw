#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "usbfw.h"

APP_CONTEXT app = {	.cmd		= APPCMD_NONE,
			.filename	= DEFAULT_OUT_FILENAME,
			.file		= NULL,
			.is_dev		= false,
			.vid		= 0,
			.pid		= 0,
			.lun		= 0,
			.lba		= 0,
			.bc		= 1,
			.is_logical	= true,
			.is_detach	= false
};

USB_BULK_CONTEXT uctx;
CBW test_cbw;

#define sectors  (1024 * 64)
uint8_t fw[SECTOR_SIZE * sectors];


void action_dev(void) {
	int err;
	uint8_t resp;

	command_init_act_init(&test_cbw);
	err = command_perform_act_init(&test_cbw, &uctx, &resp);
	if (err) {
		return;
	}
	printf("INIT Response: 0x%02hhX\n", resp);


	ACTIONSUSBD actid;
	command_init_act_identify(&test_cbw, 1);
	err = command_perform_act_identify(&test_cbw, &uctx, &actid);
	if (err) {
		goto error;
	}
	printf("Gathered ID: %11s 0x%02hhX 0x%02hhX\n", actid.actionsusbd, actid.adfu, actid.unknown);

	usleep(100);
	FILE *fwf;


	memset(fw, 0, sizeof(fw));
	printf("Dumping first logical %i sectors to `fw_log.bin`:\n", sectors);
	for (uint32_t i = 0; i < sectors; i++) {
		if ((i & 0x7F) == 0) {
			printf(".");
		}
		command_init_act_readone(&test_cbw, i, true);
		err = command_perform_act_readone(&test_cbw, &uctx, ((uint8_t *)&fw) + (i * SECTOR_SIZE));
		if (err) {
			goto error;
		}
	}
	fwf = fopen("fw_log.bin", "w");
	fwrite(&fw, SECTOR_SIZE * sectors, 1, fwf);
	fclose(fwf);
	printf("\nDONE\n");


	memset(fw, 0, sizeof(fw));
	printf("Dumping first phisical %i sectors to `fw_phy.bin`:\n", sectors);
	for (uint32_t i = 0; i < sectors; i++) {
		if ((i & 0x7F) == 0) {
			printf(".");
		}
		command_init_act_readone(&test_cbw, i, false);
		err = command_perform_act_readone(&test_cbw, &uctx, ((uint8_t *)&fw) + (i * SECTOR_SIZE));
		if (err) {
			goto error;
		}
	}
	fwf = fopen("fw_phy.bin", "w");
	fwrite(&fw, SECTOR_SIZE * sectors, 1, fwf);
	fclose(fwf);
	printf("\nDONE\n");


	memset(fw, 0, sizeof(fw));
	printf("Dumping RAM to `fw_ram.bin`:\n");
	for (uint32_t i = 0; i < 0x800; i++) {
		if ((i & 0x7F) == 0) {
			printf(".");
		}
		// If you get garbage dump, your device doesn't support RAM read. You can only read
		// sysinfo. Set then size to 192 bytes - longer reads are ignored. Sector number is
		// always ignored i this case.
		command_init_act_read_ram(&test_cbw, i, 512 /*192*/);
		err = command_perform_act_read_ram(&test_cbw, &uctx, ((uint8_t *)&fw) + (i * SECTOR_SIZE));
		if (err) {
			goto error;
		}
	}
	fwf = fopen("fw_phy.ram", "w");
	fwrite(&fw, SECTOR_SIZE * 0x800, 1, fwf);
	fclose(fwf);
	printf("\nDONE\n");


error:
	command_init_act_detach(&test_cbw);
	err = command_perform_act_detach(&test_cbw, &uctx);
	if (err) {
		return;
	}
	printf("DETACH OK\n");
}

int main (int argc, char *argv[]) {
	libusb_device **list;
	int err;
	ssize_t cnt;

	parseparams(argc, argv);

	//unbuffer stdout
	setvbuf(stdout, NULL, _IONBF, 0);

	err = libusb_init(NULL);
	if (err < 0) {
		fprintf(stderr, "Init err: %i\n", err);
	}

	cnt = libusb_get_device_list(NULL, &list);
	if (cnt < 0) {
		fprintf(stderr, "Count: %li\n", cnt);
	}

	for (uint32_t i = 0; i < cnt; i++) {

		err = init_bulk_context(&uctx, list[i]);
		if (err) {
			if (err != -1) {
				printf("Init context USB Error: %s\n", libusb_strerror((enum libusb_error)err));
			}
			goto next;
		}

		err = claim_bulk_context(&uctx);
		if (err) {
			printf("Claim context USB Error: %s\n", libusb_strerror((enum libusb_error)err));
			goto next;
		}


		SCSI_INQUIRY test_inquiry;
		command_init_inquiry(&test_cbw);
		err = command_perform_inquiry(&test_cbw, &uctx, &test_inquiry);
		if (!err) {
			char tbuf[100];

			memset(tbuf, 0x20, 100);
			tbuf[99] = 0;
			memcpy(tbuf, test_inquiry.vendor, 28);
			printf("DEVICE %04X:%04X %s\n", uctx.dev_descr.idVendor, uctx.dev_descr.idProduct, tbuf);
		}


		ACTIONSUSBD actid;
		command_init_act_identify(&test_cbw, 1);
		err = command_perform_act_identify(&test_cbw, &uctx, &actid);
		if (!err) {
			printf("Gathered ID: %11s 0x%02hhX 0x%02hhX\n", actid.actionsusbd, actid.adfu, actid.unknown);
			if (strncmp(actid.actionsusbd, "ACTIONSUSBD", 11) == 0) {
				action_dev();
			}
		}



next:
		free_bulk_context(&uctx);
	}

	libusb_free_device_list(list, 1);
	libusb_exit(NULL);
}
