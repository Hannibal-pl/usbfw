#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>

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

bool enumerate_devices(void) {
	libusb_device **list;
	enum libusb_error usb_error;
	ssize_t cnt;
	int found = 0;

	cnt = libusb_get_device_list(NULL, &list);
	if (cnt < 0) {
		printf("Error: Cannot enumerate USB devices: %s\n", libusb_strerror((enum libusb_error)cnt));
		return false;
	}

	printf("\nChecking for Actions Semiconductor compatible devices...\n\n");

	for (uint32_t i = 0; i < cnt; i++) {

		// device is not mass storage or error
		if (init_bulk_context(&uctx, list[i])) {
			free_bulk_context(&uctx);
			continue;
		}

		usb_error = claim_bulk_context(&uctx);
		if (usb_error) {
			printf("Error: Cannot claim USB endpoint: %s\n", libusb_strerror(usb_error));
			free_bulk_context(&uctx);
			continue;
		}

		printf("Testing USB mass storage device %04hX:%04hX - ", uctx.dev_descr.idVendor, uctx.dev_descr.idProduct);

		ACTIONSUSBD actid;
		command_init_act_identify(&test_cbw, 1);
		if (command_perform_act_identify(&test_cbw, &uctx, &actid)) {
			printf(COLOR_RED"FAIL"COLOR_DEFAULT"\n");
		} else if (strncmp(actid.actionsusbd, "ACTIONSUSBD", 11) != 0) {
			printf(COLOR_RED"FAIL"COLOR_DEFAULT"\n");
		} else {
			printf(COLOR_GREEN"FOUND"COLOR_DEFAULT"\n");
			found++;
		}
		free_bulk_context(&uctx);
	}

	if (found) {
		printf("\nFound %i compatible device(s).\n", found);
	} else {
		printf("\nNo compatible devices found.\n");
	}

	libusb_free_device_list(list, 1);
	return true;
}

bool scsi_inquiry(void) {
	enum libusb_error usb_error;

	if (!open_device(&uctx, app.vid, app.pid)) {
		printf("Error: Cannot opend device %04hX:%04hX.\n", app.vid, app.pid);
		return false;
	}

	usb_error = claim_bulk_context(&uctx);
	if (usb_error) {
		printf("Error: Cannot claim USB endpoint: %s\n", libusb_strerror(usb_error));
		free_bulk_context(&uctx);
		return false;
	}

	// We inted to use standard SCSI command, so there is no need to check for actions device

	printf("\nSending SCSI INQUIRY command to the device %04X:%04X LUN:%i\n\n", app.vid, app.pid, app.lun);

	SCSI_INQUIRY inquiry;
	command_init_inquiry(&test_cbw, app.lun);
	if (command_perform_inquiry(&test_cbw, &uctx, &inquiry)) {
		printf("Error: Inquiry command fail.\n");
		return false;
	}

	printf("Recieved information:\n");
	printf("Peripheral Device Type : %i (%s).\n", inquiry.pdt, decode_pdt(inquiry.pdt));
	printf("          Is removable : %s\n", inquiry.is_removable ? "NO" : "YES");
	printf("           ISO version : %i\n", inquiry.iso_version);
	printf("          ECMA version : %i\n", inquiry.ecma_version);
	printf("          ANSI version : %i\n", inquiry.ansi_version);
	printf("  Response data format : %i\n", inquiry.rdt);
	printf("                Vendor : %.7s\n", inquiry.vendor);
	printf("               Product : %.15s\n", inquiry.product);
	printf("              Revision : %.3s\n", inquiry.rev);
	printf("\n");

	return true;
}

bool scsi_read_capacity(void) {
	if (!open_device(&uctx, app.vid, app.pid)) {
		printf("Error: Cannot opend device %04hX:%04hX.\n", app.vid, app.pid);
		return false;
	}

	// We inted to use standard SCSI command, so there is no need to check for actions device

	printf("\nSending SCSI READ CAPACITY command to the device %04X:%04X LUN:%i\n\n", app.vid, app.pid, app.lun);

	SCSI_CAPACITY capacity;
	command_init_read_capacity(&test_cbw, app.lun);
	if (command_perform_read_capacity(&test_cbw, &uctx, &capacity)) {
		printf("Error: Read capacity command fail.\n");
		return false;
	}

	printf("Reported capacity is %u blocks of size %u bytes (%s).\n\n", capacity.lastLBA, capacity.blockSize, humanize_size((uint64_t)capacity.lastLBA * (uint64_t)capacity.blockSize));

	return true;
}

bool action_headinfo(void) {
	enum libusb_error usb_error;
	bool retval = false;

	if (!open_device(&uctx, app.vid, app.pid)) {
		printf("Error: Cannot opend device %04hX:%04hX.\n", app.vid, app.pid);
		return false;
	}

	usb_error = claim_bulk_context(&uctx);
	if (usb_error) {
		printf("Error: Cannot claim USB endpoint: %s\n", libusb_strerror(usb_error));
		free_bulk_context(&uctx);
		return false;
	}

	// check for Actions device
	ACTIONSUSBD actid;
	command_init_act_identify(&test_cbw, 1);
	if (command_perform_act_identify(&test_cbw, &uctx, &actid)) {
		printf("Error: Cannot identify Actions device.\n");
		retval = false;
		goto exit;
	} else if (strncmp(actid.actionsusbd, "ACTIONSUSBD", 11) != 0) {
		printf("Error: Actions indentifier not match\n");
		retval = false;
		goto exit;
	}

	// init firmware mode
	uint8_t resp;
	command_init_act_init(&test_cbw);
	if ((command_perform_act_init(&test_cbw, &uctx, &resp)) || (resp != 0xFF)) {
		printf("Error: Unable to init firmware mode\n");
		retval = false;
		goto exit;
	}

	printf("\nReading ACTIONS firmware header from device %04X:%04X LUN:%i\n\n", app.vid, app.pid, app.lun);

	FW_HEADER fw_header;
	for (uint32_t i = 0; i < 16; i++) {
		command_init_act_readone(&test_cbw, i, true);
		if (command_perform_act_readone(&test_cbw, &uctx, ((uint8_t *)&fw_header) + (i * SECTOR_SIZE))) {
			printf("Error: Reading header failed at sector %i\n", i);
			retval = false;
			goto exit;
		}
	}

	if (fw_header.magic != 0x0FF0AA55) {
		printf("Error: Readed data is isn't proper actions firmware header.\n");
		retval = false;
		goto exit;
	}

	char buf[128];
	wcstombs(buf, (wchar_t *)fw_header.bString, sizeof(buf));

	printf("Recieved information:\n\n");
	printf("               Version : %01hhX.%01hhX.%02hhX.%0hhX%0hhX\n", fw_header.version[0] >> 4, fw_header.version[0] & 0xF, fw_header.version[1], fw_header.version[2], fw_header.version[3]);
	printf("                  Date : %02hhX%02hhX.%02hhX.%02hhX\n", fw_header.date[0], fw_header.date[1], fw_header.date[2], fw_header.date[3]);
	printf("             Vendor ID : 0x%04hX\n", fw_header.vendorId);
	printf("            Product ID : 0x%04hX\n", fw_header.productId);
	printf("    Directory Checksum : 0x%08X\n", fw_header.dirCheckSum);
	printf("   Firmware Descriptor : %.44s\n", fw_header.fwDescriptor);
	printf("              Producer : %.32s\n", fw_header.producer);
	printf("           Device Name : %.32s\n", fw_header.deviceName);
	printf("         USB Attribute : %.8s\n", fw_header.usbAttri);
	printf("    USB Identification : %.16s\n", fw_header.usbIdentification);
	printf("   USB Product Version : %.4s\n", fw_header.usbProductVer);
	printf("              USB Name : %.46s\n", buf);
	printf(" MTP Manufacturer Info : %.32s\n", fw_header.mtpManufacturerInfo);
	printf("      MTP Product Info : %.32s\n", fw_header.mtpProductInfo);
	printf("   MTP Product Version : %.32s\n", fw_header.mtpProductVersion);
	printf("        MTP Product SN : %.32s\n", fw_header.mtpProductSerialNumber);
	printf("         MTP Vendor ID : 0x%04hX\n", fw_header.mtpVendorId);
	printf("        MTP Product ID : 0x%04hX\n", fw_header.mtpProductId);
	printf("       Header Checksum : 0x%04hX\n", fw_header.headerChecksum);
	printf("\nCommon Values:\n\n");
	printf(" System Time (in 0.5s) : 0x%08X\n", fw_header.defaultInf.systemtime);
	printf("\n");

	retval = true;

exit:
	if (app.is_detach) {
		printf("Detaching device ");
		command_init_act_detach(&test_cbw);
		if (command_perform_act_detach(&test_cbw, &uctx)) {
			printf("failed.\n");
		} else {
			printf("succesful.\n");
		}
	}
	return retval;
}

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
	enum libusb_error usb_error = 0;
	int retval;

	parseparams(argc, argv);

	usb_error = libusb_init(NULL);
	if (usb_error) {
		printf("Error: libusb int: %s.\n", libusb_strerror(usb_error));
		return -1;
	}


	switch (app.cmd) {
		case APPCMD_ENUMERATE:
			retval = enumerate_devices() ? 0 : 1;
			break;
		case APPCMD_INQUIRY:
			retval = scsi_inquiry() ? 0 : 1;
			break;
		case APPCMD_CAPACITY:
			retval = scsi_read_capacity() ? 0 : 1;
			break;
		case APPCMD_HEADINFO:
			retval = action_headinfo() ? 0 : 1;
			break;
		default:
			printf("Error: Unknown command.\n");
			retval = -1;
	}

	libusb_exit(NULL);
	return retval;
}


/*	//unbuffer stdout
	setvbuf(stdout, NULL, _IONBF, 0);

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

	libusb_free_device_list(list, 1);*/
