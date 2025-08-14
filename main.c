#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>

#include "usbfw.h"

APP_CONTEXT app = {	.cmd		= APPCMD_NONE,
			.ofilename	= DEFAULT_OUT_FILENAME,
			.ofile		= NULL,
			.ifilename	= DEFAULT_IN_FILENAME,
			.ifile		= NULL,
			.offset		= 0,
			.is_dev		= false,
			.vid		= 0,
			.pid		= 0,
			.lun		= 0,
			.lba		= 0,
			.bc		= 1,
			.is_logical	= true,
			.is_showdir	= false,
			.is_detach	= false,
			.is_alt_fw	= false,
			.is_yesiknow	= false,
			.entry_param	= 0
};

USB_BULK_CONTEXT uctx;
CBW cbw;

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
	if (!open_and_claim(&uctx, app.vid, app.pid)) {
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

	printf("Recieved information:\n\n");
	printf("Peripheral Device Type : %i (%s).\n", inquiry.pdt, decode_pdt(inquiry.pdt));
	printf("          Is removable : %s\n", inquiry.is_removable ? "NO" : "YES");
	printf("           ISO version : %i\n", inquiry.iso_version);
	printf("          ECMA version : %i\n", inquiry.ecma_version);
	printf("          ANSI version : %i\n", inquiry.ansi_version);
	printf("  Response data format : %i\n", inquiry.rdt);
	printf("                Vendor : %.8s\n", inquiry.vendor);
	printf("               Product : %.16s\n", inquiry.product);
	printf("              Revision : %.4s\n", inquiry.rev);
	printf("\n");

	return true;
}

bool scsi_read_capacity(void) {
	if (!open_and_claim(&uctx, app.vid, app.pid)) {
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

bool scsi_read10(void) {
	bool retval = false;

	if (!open_and_claim(&uctx, app.vid, app.pid)) {
		return false;
	}

	// We inted to use standard SCSI command, so there is no need to check for actions device

	// read capacity first to know drive geometry and sector size
	SCSI_CAPACITY capacity;
	command_init_read_capacity(&cbw, app.lun);
	if (command_perform_read_capacity(&cbw, &uctx, &capacity)) {
		printf("Error: Read capacity command fail.\n");
		return false;
	}

	if (capacity.blockSize > SECTOR_SIZE) {
		printf("Error: Sector size %u grater than max supported %u.\n", capacity.blockSize, SECTOR_SIZE);
		return false;
	}

	if (app.lba >= capacity.lastLBA) {
		printf("Error: LBA should be less than (%u) 0x%08X.\n", capacity.lastLBA, capacity.lastLBA);
		return false;
	}

	if (app.lba + app.bc > capacity.lastLBA) {
		printf("Error: LBA + block count should be less or equal (%u) 0x%08X.\n", capacity.lastLBA, capacity.lastLBA);
		return false;
	}

	printf("\nReading from mass storage SCSI device %04X:%04X LUN:%i to file \"%s\",\n", app.vid, app.pid, app.lun, app.ofilename);
	printf("starting at sector 0x%08X and ending at sector 0x%08X (0x%08X sectors total).\n\n", app.lba , app.lba + app.bc - 1, app.bc);

	app.ofile = fopen(app.ofilename, "w");
	if (!app.ofile) {
		printf("Error: Cannot open output file \"%s\".", app.ofilename);
		retval = false;
		goto exit;
	}

	printf("Reading mass storage ...       ");
	uint8_t dumpbuffer[SECTOR_SIZE];
	for (uint32_t i = app.lba; i < app.lba + app.bc; i++) {
		command_init_read10one(&cbw, app.lun, i, capacity.blockSize);
		if (command_perform_read10one(&cbw, &uctx, (uint8_t *)&dumpbuffer)) {
			printf("Error: Reading mass storage failed at sector %i\n", i);
			retval = false;
			goto exit;
		}
		fwrite(dumpbuffer, SECTOR_SIZE, 1, app.ofile);

		if ((i & 0xF) == 0) {
			display_percent_spinner(i - app.lba, app.bc);
		}
	}
	printf("\b\b\b\b\bdone.\n\n");

exit:
	if (app.ofile) {
		fclose(app.ofile);
		app.ofile = NULL;
	}
	return retval;
}

bool scsi_write10(void) {
	bool retval = false;

	if (!open_and_claim(&uctx, app.vid, app.pid)) {
		return false;
	}

	// We inted to use standard SCSI command, so there is no need to check for actions device

	// read capacity first to know drive geometry and sector size
	SCSI_CAPACITY capacity;
	command_init_read_capacity(&cbw, app.lun);
	if (command_perform_read_capacity(&cbw, &uctx, &capacity)) {
		printf("Error: Read capacity command fail.\n");
		return false;
	}

	if (capacity.blockSize > SECTOR_SIZE) {
		printf("Error: Sector size %u grater than max supported %u.\n", capacity.blockSize, SECTOR_SIZE);
		return false;
	}

	if (app.lba >= capacity.lastLBA) {
		printf("Error: LBA should be less than (%u) 0x%08X.\n", capacity.lastLBA, capacity.lastLBA);
		return false;
	}

	if (app.lba + app.bc > capacity.lastLBA) {
		printf("Error: LBA + block count should be less or equal (%u) 0x%08X.\n", capacity.lastLBA, capacity.lastLBA);
		return false;
	}

	app.ifile = fopen(app.ofilename, "r");
	if (!app.ifile) {
		printf("Error: Cannot open output file \"%s\".", app.ifilename);
		retval = false;
		goto exit;
	}

	fseek(app.ifile, 0, SEEK_END);
	uint32_t oflen = ftell(app.ifile);

	if (app.offset > oflen) {
		printf("Error: Provided offset is greater than input file length.");
		retval = false;
		goto exit;
	}
	fseek(app.ifile, app.offset, SEEK_SET);

	if ((oflen - app.offset) < (app.bc * capacity.blockSize)) {
		printf("Error: Not enough data in input file.");
		retval = false;
		goto exit;
	}

	printf("\nWriting to mass storage SCSI device %04X:%04X LUN:%i from file \"%s\",\n", app.vid, app.pid, app.lun, app.ofilename);
	printf("starting at sector 0x%08X and ending at sector 0x%08X (0x%08X sectors total).\n\n", app.lba , app.lba + app.bc - 1, app.bc);

	printf("Writing mass storage ...       ");
	uint8_t inbuffer[SECTOR_SIZE];
	for (uint32_t i = app.lba; i < app.lba + app.bc; i++) {
		fread(inbuffer, SECTOR_SIZE, 1, app.ifile);
		command_init_write10one(&cbw, app.lun, i, capacity.blockSize);
		if (command_perform_write10one(&cbw, &uctx, (uint8_t *)&inbuffer)) {
			printf("Error: Writing mass storage failed at sector %i\n", i);
			retval = false;
			goto exit;
		}

		if ((i & 0xF) == 0) {
			display_percent_spinner(i - app.lba, app.bc);
		}
	}
	printf("\b\b\b\b\bdone.\n\n");

exit:
	if (app.ifile) {
		fclose(app.ifile);
		app.ifile = NULL;
	}
	return retval;
}

bool action_headinfo(void) {
	bool retval = false;
	uint32_t first_sector = 0;

	if (!open_and_claim(&uctx, app.vid, app.pid)) {
		return false;
	}

	if (!init_act(&uctx)) {
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
	if (!get_fw_header(&uctx, &fw_header, app.lun, first_sector)) {
		retval = false;
		goto exit;
	}

	printf("Recieved information:\n\n");
	printf("               Version : %01hhX.%01hhX.%02hhX.%02hhX%02hhX\n",
		fw_header.version[0] >> 4,
		fw_header.version[0] & 0xF,
		fw_header.version[1],
		fw_header.version[2],
		fw_header.version[3]);
	printf("                  Date : %02hhX%02hhX.%02hhX.%02hhX\n",
		fw_header.date[0],
		fw_header.date[1],
		fw_header.date[2],
		fw_header.date[3]);
	printf("             Vendor ID : 0x%04hX\n", fw_header.vendorId);
	printf("            Product ID : 0x%04hX\n", fw_header.productId);
	printf("    Directory Checksum : 0x%08X - %s\n", fw_header.dirCheckSum, (checksum32((uint32_t *) fw_header.diritem, sizeof(FW_DIR_ENTRY) * 240, true) == fw_header.dirCheckSum) ? "OK" : "Error");
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
	printf("       Header Checksum : 0x%04hX - %s\n", fw_header.headerChecksum, (checksum16((uint16_t *)&fw_header, 510, true) == fw_header.headerChecksum) ? "OK" : "Error");
	printf("\nCommon Values:\n\n");
	printf("                 Magic : 0x%04hX - %s\n", fw_header.defaultInf.magic, fw_header.defaultInf.magic == 0xDEAD ? "OK" : "Error");
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

		printf("    Filename:       Checksum:    Attributes:  Version:  Offset in sectors:  Length in bytes:\n\n");
		for (uint32_t i = 0; i < 240; i++) {
			FW_DIR_ENTRY *entry = &fw_header.diritem[i];
			//skip empty entries
			if (entry->filename[0] != 0) {
				printf("    %s    0x%08X   0x%02hhX         0x%04hX    0x%08X          0x%08X (%s)\n", 
					make_filename(entry->filename),
					entry->checksum,
					entry->attr,
					entry->version,
					entry->offset,
					entry->length, humanize_size(entry->length));
			}
		}
	}

	printf("\n");

	retval = true;

exit:
	detach_device(&uctx, app.is_detach);
	return retval;
}


bool action_sysinfo(void) {
	bool retval = false;

	if (!open_and_claim(&uctx, app.vid, app.pid)) {
		return false;
	}

	if (!init_act(&uctx)) {
		retval = false;
		goto exit;
	}

	printf("\nReading ACTIONS firmware sysinfo structure from device %04X:%04X\n\n", app.vid, app.pid);

	FW_SYSINFO sysinfo;
	if (!get_fw_sysinfo(&uctx, &sysinfo)) {
		retval = false;
		goto exit;
	}

	printf("Recieved information:\n\n");
	printf("            IC Version : %04hX\n", sysinfo.hwScan.icVersion);
	printf("            SubVersion : %c%c\n", sysinfo.hwScan.subversion[0], sysinfo.hwScan.subversion[1]);
	printf("          BROM Version : %01hhX.%01hhX.%02hhX.%02hhX%02hhX\n",
		sysinfo.hwScan.bromVersion[0] >> 4,
		sysinfo.hwScan.bromVersion[0] & 0xF,
		sysinfo.hwScan.bromVersion[1],
		sysinfo.hwScan.bromVersion[2],
		sysinfo.hwScan.bromVersion[3]);
	printf("             BROM Date : %02hhX%02hhX.%02hhX.%02hhX\n",
		sysinfo.hwScan.bromDate[0],
		sysinfo.hwScan.bromDate[1],
		sysinfo.hwScan.bromDate[2],
		sysinfo.hwScan.bromDate[3]);
	printf("        Boot Disk Type : %.4s\n", sysinfo.hwScan.bootDiskType);
	printf("     Storage Conn Info : 0x%04hX 0x%04hX 0x%04hX 0x%04hX\n",
		sysinfo.hwScan.stgInfo.connInfo[0],
		sysinfo.hwScan.stgInfo.connInfo[1],
		sysinfo.hwScan.stgInfo.connInfo[2],
		sysinfo.hwScan.stgInfo.connInfo[3]);
	printf("  Storage Capabilities : 0x%04hX 0x%04hX 0x%04hX 0x%04hX 0x%04hX 0x%04hX 0x%04hX 0x%04hX\n",
		sysinfo.hwScan.stgInfo.caps[0],
		sysinfo.hwScan.stgInfo.caps[1],
		sysinfo.hwScan.stgInfo.caps[2],
		sysinfo.hwScan.stgInfo.caps[3],
		sysinfo.hwScan.stgInfo.caps[4],
		sysinfo.hwScan.stgInfo.caps[5],
		sysinfo.hwScan.stgInfo.caps[6],
		sysinfo.hwScan.stgInfo.caps[7]);
	printf("             Vendor ID : %04hX\n", sysinfo.fwScan.vendorId);
	printf("            Product ID : %04hX\n", sysinfo.fwScan.productId);
	printf("      Firmware Version : %04hX\n", sysinfo.fwScan.firmwareVersion);
	printf("              Producer : %.32s\n", sysinfo.fwScan.producer);
	printf("           Device Name : %.32s\n", sysinfo.fwScan.deviceName);


	printf("\n");

	retval = true;

exit:
	detach_device(&uctx, app.is_detach);
	return retval;
}


bool action_readfw(void) {
	bool retval = false;

	if (!open_and_claim(&uctx, app.vid, app.pid)) {
		return false;
	}

	if (!init_act(&uctx)) {
		retval = false;
		goto exit;
	}

	printf("\nReading ACTIONS firmware %s area from device %04X:%04X LUN:%i to file \"%s\",\n", app.is_logical ? "logical" : "physical", app.vid, app.pid, app.lun, app.ofilename);
	printf("starting at sector 0x%08X and ending at sector 0x%08X (0x%08X sectors total).\n\n", app.lba , app.lba + app.bc - 1, app.bc);

	app.ofile = fopen(app.ofilename, "w");
	if (!app.ofile) {
		printf("Error: Cannot open output file \"%s\".", app.ofilename);
		retval = false;
		goto exit;
	}

	printf("Reading firmware ...      ");
	uint8_t dumpbuffer[SECTOR_SIZE];
	for (uint32_t i = app.lba; i < app.lba + app.bc; i++) {
		command_init_act_readone(&cbw, app.lun, i, app.is_logical);
		if (command_perform_act_readone(&cbw, &uctx, (uint8_t *)&dumpbuffer)) {
			printf("Error: Reading firmware failed at sector %i\n", i);
			retval = false;
			goto exit;
		}
		fwrite(dumpbuffer, SECTOR_SIZE, 1, app.ofile);

		if ((i & 0xF) == 0) {
			display_percent_spinner(i - app.lba, app.bc);
		}
	}
	printf("\b\b\b\b\bdone.\n\n");


exit:
	if (app.ofile) {
		fclose(app.ofile);
		app.ofile = NULL;
	}

	detach_device(&uctx, app.is_detach);
	return retval;
}

bool action_test_ramacc(void) {
	bool retval = false;

	if (!open_and_claim(&uctx, app.vid, app.pid)) {
		return false;
	}

	if (!init_act(&uctx)) {
		retval = false;
		goto exit;
	}

	printf("\nGeneral RAM access is %s.\n\n", test_ram_access(&uctx) ? "possible" : "impossible");

exit:
	detach_device(&uctx, app.is_detach);
	return retval;
}

bool action_readram(void) {
	bool retval = false;

	if (app.lba >= 0x800) {
		printf("Error: LBA should be less than (2048) 0x800.\n");
		return false;
	}

	if (app.lba + app.bc > 0x800) {
		printf("Error: LBA + block count should be less or equal (2048) 0x800.\n");
		return false;
	}

	if (!open_and_claim(&uctx, app.vid, app.pid)) {
		return false;
	}

	if (!init_act(&uctx)) {
		retval = false;
		goto exit;
	}

	printf("\nReading ACTIONS device %04X:%04X RAM to file \"%s\",\n", app.vid, app.pid, app.ofilename);
	printf("starting at sector 0x%08X and ending at sector 0x%08X (0x%08X sectors total).\n\n", app.lba, app.lba + app.bc - 1, app.bc);

	if (!test_ram_access(&uctx)) {
		printf("Warning: Your device probably doesn't support this feature. Expect garbage output.\n\n");
	}

	app.ofile = fopen(app.ofilename, "w");
	if (!app.ofile) {
		printf("Error: Cannot open output file \"%s\".", app.ofilename);
		retval = false;
		goto exit;
	}

	printf("Reading RAM ...  ");
	uint8_t dumpbuffer[SECTOR_SIZE];
	for (uint32_t i = app.lba; i < app.lba + app.bc; i++) {
		command_init_act_read_ram(&cbw, i, SECTOR_SIZE);
		if (command_perform_act_read_ram(&cbw, &uctx, (uint8_t *)&dumpbuffer)) {
			printf("Error: Reading RAM failed at sector %i\n", i);
			retval = false;
			goto exit;
		}
		fwrite(dumpbuffer, SECTOR_SIZE, 1, app.ofile);

		if ((i & 0xF) == 0) {
			display_spinner();
		}
	}
	printf("\bdone.\n\n");


exit:
	if (app.ofile) {
		fclose(app.ofile);
		app.ofile = NULL;
	}

	detach_device(&uctx, app.is_detach);
	return retval;
}

bool action_dumpraw(void) {
	bool retval = false;
	uint32_t first_sector = 0;
	uint32_t size = 0;

	if (!open_and_claim(&uctx, app.vid, app.pid)) {
		return false;
	}

	if (!init_act(&uctx)) {
		retval = false;
		goto exit;
	}

	app.ofile = fopen(app.ofilename, "w");
	if (!app.ofile) {
		printf("Error: Cannot open output file \"%s\".", app.ofilename);
		retval = false;
		goto exit;
	}


	if (app.is_logical) {
		//main firmware
		printf("\nDumping ACTIONS main firmware (%s) from device %04X:%04X LUN:%i to file \"%s\".\n\n", app.is_alt_fw ? "alternate" : "main", app.vid, app.pid, app.lun, app.ofilename);

		if (app.is_alt_fw) {
			first_sector = search_alternate_fw(&uctx, app.lun, MAX_SEARCH_LBA);
			// exit if error or not found
			if ((first_sector == 0xFFFFFFFF) || (first_sector == 0xFFFFFFFF)) {
				retval = false;
				goto exit;
			}
		}

		size = get_fw_size(&uctx, 0, first_sector);

	} else {
		//bootrecord firmware
		printf("\nDumping ACTIONS bootrecord firmware (%s) from device %04X:%04X LUN:%i to file \"%s\".\n\n", app.is_alt_fw ? "alternate" : "main", app.vid, app.pid, app.lun, app.ofilename);

		if (app.is_alt_fw) {
			first_sector = 0x200; // alternate begins at 0x40000
		}
		size = 0x20; // botrecord has 0x4000 bytes
	}

	printf("Reading firmware ...      ");
	uint8_t dumpbuffer[SECTOR_SIZE];
	for (uint32_t i = first_sector; i < first_sector + size; i++) {
		command_init_act_readone(&cbw, app.lun, i, app.is_logical);
		if (command_perform_act_readone(&cbw, &uctx, (uint8_t *)&dumpbuffer)) {
			printf("Error: Reading firmware failed at sector %i.\n", i);
			retval = false;
			goto exit;
		}
		fwrite(dumpbuffer, SECTOR_SIZE, 1, app.ofile);

		if ((i & 0xF) == 0) {
			display_percent_spinner(i - first_sector, size);
		}
	}
	printf("\b\b\b\b\bdone.\n\n");

exit:
	if (app.ofile) {
		fclose(app.ofile);
		app.ofile = NULL;
	}

	detach_device(&uctx, app.is_detach);
	return retval;
}

bool action_dumpafi(void) {
	bool retval = false;
	uint32_t first_sector = 0;
	uint32_t size = 0;

	if (!open_and_claim(&uctx, app.vid, app.pid)) {
		return false;
	}

	if (!init_act(&uctx)) {
		retval = false;
		goto exit;
	}

	app.ofile = afi_new_file(app.ofilename);
	if (!app.ofile) {
		printf("Error: Cannot open output file \"%s\".", app.ofilename);
		retval = false;
		goto exit;
	}

	printf("\nDumping ACTIONS whole firmware (%s) from device %04X:%04X LUN:%i to AFI file \"%s\".\n\n", app.is_alt_fw ? "alternate" : "main", app.vid, app.pid, app.lun, app.ofilename);

	// bootrecord firmware
	if (app.is_alt_fw) {
		first_sector = 0x200; // alternate begins at 0x40000
	} else {
		first_sector = 0; // alternate begins at 0x40000
	}
	size = 0x20; // botrecord has 0x4000 bytes

	printf("Reading bootrecord firmware ...      ");
	FW_BREC fw_brec;
	for (uint32_t i = first_sector; i < first_sector + size; i++) {
		command_init_act_readone(&cbw, app.lun, i, false);
		if (command_perform_act_readone(&cbw, &uctx, (uint8_t *)&fw_brec + ((i - first_sector) * SECTOR_SIZE))) {
			printf("Error: Reading firmware failed at sector %i.\n", i);
			retval = false;
			goto exit;
		}

		if ((i & 0xF) == 0) {
			display_percent_spinner(i - first_sector, size);
		}
	}
	printf("\b\b\b\b\bdone.\n\n");

	// put bootrecord in afi container as whole file
	FW_AFI_DIR_ENTRY dir_entry;
	memset(&dir_entry, 0, sizeof(FW_AFI_DIR_ENTRY));
	memcpy(dir_entry.filename, "BREC    BIN", 11);
	memcpy(&dir_entry.filename[4], fw_brec.type, 4);
	dir_entry.type = 'B';
	dir_entry.downloadAddr = AFI_DADDR_B;
	dir_entry.length = sizeof(FW_BREC);
	afi_add_whole(app.ofile, &dir_entry, (uint8_t *)&fw_brec);

	//main firmware
	if (app.is_alt_fw) {
		first_sector = search_alternate_fw(&uctx, app.lun, MAX_SEARCH_LBA);
		// exit if error or not found
		if ((first_sector == 0xFFFFFFFF) || (first_sector == 0xFFFFFFFF)) {
			retval = false;
			goto exit;
		}
	} else {
		first_sector = 0;
	}
	size = get_fw_size(&uctx, 0, first_sector);


	// prepare out file for data
	fseek(app.ofile, 0, SEEK_END);
	uint32_t checksum = 0;

	printf("Reading main firmware ...      ");
	uint8_t dumpbuffer[SECTOR_SIZE];
	for (uint32_t i = first_sector; i < first_sector + size; i++) {
		command_init_act_readone(&cbw, app.lun, i, true);
		if (command_perform_act_readone(&cbw, &uctx, (uint8_t *)&dumpbuffer)) {
			printf("Error: Reading firmware failed at sector %i.\n", i);
			retval = false;
			goto exit;
		}
		fwrite(dumpbuffer, SECTOR_SIZE, 1, app.ofile);
		checksum += checksum32((uint32_t *)dumpbuffer, SECTOR_SIZE, true);

		if ((i & 0xF) == 0) {
			display_percent_spinner(i - first_sector, size);
		}
	}
	printf("\b\b\b\b\bdone.\n\n");

	// put main firmware in afi container as previusly appended
	memset(&dir_entry, 0, sizeof(FW_AFI_DIR_ENTRY));
	memcpy(dir_entry.filename, "FWIMAGE FW ", 11);
	dir_entry.type = 'I';
	dir_entry.downloadAddr = AFI_DADDR_I;
	dir_entry.length = size * SECTOR_SIZE;
	dir_entry.checksum = checksum;
	afi_add_appended(app.ofile, &dir_entry);

	// sysinfo

	FW_SYSINFO sysinfo;
	if (!get_fw_sysinfo(&uctx, &sysinfo)) {
		retval = false;
		goto exit;
	}

	// put bootrecord in afi container as whole file
	memset(&dir_entry, 0, sizeof(FW_AFI_DIR_ENTRY));
	memcpy(dir_entry.filename, "SYSINFO BIN", 11);
	dir_entry.type = ' ';
	dir_entry.length = sizeof(FW_SYSINFO);
	afi_add_whole(app.ofile, &dir_entry, (uint8_t *)&sysinfo);

	printf("AFI file ready.\n\n");

exit:
	if (app.ofile) {
		fclose(app.ofile);
		app.ofile = NULL;
	}

	detach_device(&uctx, app.is_detach);
	return retval;
}

bool action_entry(void) {
	bool retval = false;

	if (!open_and_claim(&uctx, app.vid, app.pid)) {
		return false;
	}

	if (!init_act(&uctx)) {
		retval = false;
		goto exit;
	}

	if (!confirm()) {
		return false;
	}

	printf("\nRunning ACTIONS firmware entry command with param 0x%04hX to device %04X:%04X\n\n", app.entry_param, app.vid, app.pid);

	command_init_act_entry(&cbw, app.entry_param);
	if (command_perform_act_entry(&cbw, &uctx)) {
		printf("Error: Running entry command.\n");
		retval = false;
		goto exit;
	}


exit:
	detach_device(&uctx, app.is_detach);
	return retval;
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
		case APPCMD_READ:
			retval = scsi_read10() ? 0 : 1;
			break;
		case APPCMD_WRITE:
			retval = scsi_write10() ? 0 : 1;
			break;
		case APPCMD_HEADINFO:
			retval = action_headinfo() ? 0 : 1;
			break;
		case APPCMD_SYSINFO:
			retval = action_sysinfo() ? 0 : 1;
			break;
		case APPCMD_READ_FW:
			retval = action_readfw() ? 0 : 1;
			break;
		case APPCMD_TEST_RAMACC:
			retval = action_test_ramacc() ? 0 : 1;
			break;
		case APPCMD_READ_RAM:
			retval = action_readram() ? 0 : 1;
			break;
		case APPCMD_DUMP_RAW:
			retval = action_dumpraw() ? 0 : 1;
			break;
		case APPCMD_DUMP_AFI:
			retval = action_dumpafi() ? 0 : 1;
			break;
		case APPCMD_ENTRY:
			retval = action_entry() ? 0 : 1;
			break;
		default:
			printf("Error: Unknown command.\n");
			retval = -1;
	}

	libusb_exit(NULL);
	return retval;
}
