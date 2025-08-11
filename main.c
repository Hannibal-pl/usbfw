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
			.is_showdir	= false,
			.is_detach	= false,
			.is_alt_fw	= false
};

USB_BULK_CONTEXT uctx;
CBW cbw;

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
		command_init_act_identify(&cbw, 1);
		if (command_perform_act_identify(&cbw, &uctx, &actid)) {
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
	command_init_inquiry(&cbw, app.lun);
	if (command_perform_inquiry(&cbw, &uctx, &inquiry)) {
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
	command_init_read_capacity(&cbw, app.lun);
	if (command_perform_read_capacity(&cbw, &uctx, &capacity)) {
		printf("Error: Read capacity command fail.\n");
		return false;
	}

	printf("Reported capacity is %u blocks of size %u bytes (%s).\n\n", capacity.lastLBA, capacity.blockSize, humanize_size((uint64_t)capacity.lastLBA * (uint64_t)capacity.blockSize));

	return true;
}

bool action_headinfo(void) {
	enum libusb_error usb_error;
	bool retval = false;
	uint32_t first_sector = 0;

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
	command_init_act_identify(&cbw, 1);
	if (command_perform_act_identify(&cbw, &uctx, &actid)) {
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
	command_init_act_init(&cbw);
	if ((command_perform_act_init(&cbw, &uctx, &resp)) || (resp != 0xFF)) {
		printf("Error: Unable to init firmware mode\n");
		retval = false;
		goto exit;
	}

	printf("\nReading ACTIONS firmware header (%s) from device %04X:%04X LUN:%i\n\n", app.is_alt_fw ? "alternate" : "main", app.vid, app.pid, app.lun);

	if (app.is_alt_fw) {
		first_sector = search_alternate_fw(&uctx, app.lun, MAX_SEARCH_LBA);
		// exit if error or not found
		if ((first_sector == 0xFFFFFFFF) || (first_sector == 0xFFFFFFFF)) {
			retval = false;
			goto exit;
		}
	}

	FW_HEADER fw_header;
	for (uint32_t i = 0; i < 16; i++) {
		command_init_act_readone(&cbw, app.lun, first_sector + i, true);
		if (command_perform_act_readone(&cbw, &uctx, ((uint8_t *)&fw_header) + (i * SECTOR_SIZE))) {
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
	printf("              USB Name : %.46s\n", covert_usb_string_descriptor(fw_header.bString, 46));
	printf(" MTP Manufacturer Info : %.32s\n", fw_header.mtpManufacturerInfo);
	printf("      MTP Product Info : %.32s\n", fw_header.mtpProductInfo);
	printf("   MTP Product Version : %.32s\n", fw_header.mtpProductVersion);
	printf("        MTP Product SN : %.32s\n", convert_mtp_serial(fw_header.mtpProductSerialNumber));
	printf("         MTP Vendor ID : 0x%04hX\n", fw_header.mtpVendorId);
	printf("        MTP Product ID : 0x%04hX\n", fw_header.mtpProductId);
	printf("       Header Checksum : 0x%04hX\n", fw_header.headerChecksum);
	printf("\nCommon Values:\n\n");
	printf(" System Time (in 0.5s) : 0x%08X\n", fw_header.defaultInf.systemtime);
	printf("              RTC Rate : 0x%04hX\n", fw_header.defaultInf.RTCRate);
	printf("      Display Contrast : 0x%hu\n", fw_header.defaultInf.displayContrast);
	printf("            Light Time : 0x%hu\n", fw_header.defaultInf.lightTime);
	printf("          Standby Time : 0x%hu\n", fw_header.defaultInf.standbyTime);
	printf("            Sleep Time : 0x%hu\n", fw_header.defaultInf.sleepTime);
	printf("           Language ID : %i - %s\n", fw_header.defaultInf.langid, decode_langid(fw_header.defaultInf.langid));
	printf("           Replay Mode : 0x%hu\n", fw_header.defaultInf.replayMode);
	printf("           Online Mode : 0x%hu\n", fw_header.defaultInf.onlineMode);
	printf("          Battery Type : %i - %s\n", fw_header.defaultInf.batteryType, decode_battery(fw_header.defaultInf.batteryType));
	printf("           FM Build in : %i - %s\n", fw_header.defaultInf.fmBuildIn, fw_header.defaultInf.fmBuildIn ? "YES" : "NO");

	if (app.is_showdir) {
		printf("\nFiles:\n\n");

		printf("    Filename:       Attributes:  Version:  Offset in sectors:  Length in bytes:  Checksum:\n\n");
		for (uint32_t i = 0; i < 240; i++) {
			FW_DIR_ENTRY *entry = &fw_header.diritem[i];
			//skip empty entries
			if (entry->filename[0] != 0) {
				printf("    %s    0x%02hhX         0x%04hX    0x%08X          0x%08X        0x%08X\n", make_filename(entry->filename), entry->attr, entry->version, entry->offset, entry->length, entry->checksum);
			}
		}
	}

	printf("\n");

	retval = true;

exit:
	if (app.is_detach) {
		printf("Detaching device ");
		command_init_act_detach(&cbw);
		if (command_perform_act_detach(&cbw, &uctx)) {
			printf("failed.\n");
		} else {
			printf("succesful.\n");
		}
	}
	return retval;
}

bool action_readfw(void) {
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
	command_init_act_identify(&cbw, 1);
	if (command_perform_act_identify(&cbw, &uctx, &actid)) {
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
	command_init_act_init(&cbw);
	if ((command_perform_act_init(&cbw, &uctx, &resp)) || (resp != 0xFF)) {
		printf("Error: Unable to init firmware mode\n");
		retval = false;
		goto exit;
	}

	printf("\nReading ACTIONS firmware %s area from device %04X:%04X LUN:%i to file \"%s\",\n", app.is_logical ? "logical" : "physical", app.vid, app.pid, app.lun, app.filename);
	printf("starting at sector 0x%08X and ending at sector 0x%08X (0x%08X sectors total).\n\n", app.lba , app.lba + app.bc - 1, app.bc);

	app.file = fopen(app.filename, "w");
	if (!app.file) {
		printf("Error: Cannot open output file \"%s\".", app.filename);
		retval = false;
		goto exit;
	}

	printf("Reading firmware ...  ");
	uint8_t dumpbuffer[SECTOR_SIZE];
	for (uint32_t i = app.lba; i < app.lba + app.bc; i++) {
		command_init_act_readone(&cbw, app.lun, i, app.is_logical);
		if (command_perform_act_readone(&cbw, &uctx, (uint8_t *)&dumpbuffer)) {
			printf("Error: Reading firmware failed at sector %i\n", i);
			retval = false;
			goto exit;
		}
		fwrite(dumpbuffer, SECTOR_SIZE, 1, app.file);

		if ((i & 0xF) == 0) {
			display_spinner();
		}
	}
	printf("\bdone.\n\n");


exit:
	if (app.file) {
		fclose(app.file);
		app.file = NULL;
	}

	if (app.is_detach) {
		printf("Detaching device ");
		command_init_act_detach(&cbw);
		if (command_perform_act_detach(&cbw, &uctx)) {
			printf("failed.\n");
		} else {
			printf("succesful.\n");
		}
	}
	return retval;
}


/*void action_dev(void) {
	int err;
	uint8_t resp;

	command_init_act_init(&cbw);
	err = command_perform_act_init(&cbw, &uctx, &resp);
	if (err) {
		return;
	}
	printf("INIT Response: 0x%02hhX\n", resp);


	memset(fw, 0, sizeof(fw));
	printf("Dumping RAM to `fw_ram.bin`:\n");
	for (uint32_t i = 0; i < 0x800; i++) {
		if ((i & 0x7F) == 0) {
			printf(".");
		}
		// If you get garbage dump, your device doesn't support RAM read. You can only read
		// sysinfo. Set then size to 192 bytes - longer reads are ignored. Sector number is
		// always ignored i this case.
		command_init_act_read_ram(&cbw, i, 512 192);
		err = command_perform_act_read_ram(&cbw, &uctx, ((uint8_t *)&fw) + (i * SECTOR_SIZE));
		if (err) {
			goto error;
		}
	}
	fwf = fopen("fw_phy.ram", "w");
	fwrite(&fw, SECTOR_SIZE * 0x800, 1, fwf);
	fclose(fwf);
	printf("\nDONE\n");


error:
}*/

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
		case APPCMD_READ_FW:
			retval = action_readfw() ? 0 : 1;
			break;
		default:
			printf("Error: Unknown command.\n");
			retval = -1;
	}

	libusb_exit(NULL);
	return retval;
}
