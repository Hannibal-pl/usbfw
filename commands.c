#include <string.h>
#include <endian.h>

#include "usbfw.h"

bool is_csw(uint8_t *buf) {
	if (buf[0] != 'U') {
		return false;
	} else if (buf[1] != 'S') {
		return false;
	} else if (buf[2] != 'B') {
		return false;
	} else if (buf[3] != 'S') {
		return false;
	}
	return true;
}


int check_csw(CBW *cbw, CSW *csw) {
	// check CSW
	if (strncmp((char *)csw->dCSWSignature, "USBS", 4)) {
		dbg_printf("CSW signature not present '%c%c%c%c' != 'USBS'\n", csw->dCSWSignature[0], csw->dCSWSignature[1], csw->dCSWSignature[2], csw->dCSWSignature[3]);
		return -1;
	}

	// Tag differs, return error
	if (csw->dCSWTag != cbw->dCBWTag) {
		dbg_printf("CSW tag mismatch %u != %u\n", csw->dCSWTag, cbw->dCBWTag);
		return -1;
	}

	dbg_printf("CSW Status 0x%02hhX - %s\n", csw->bCSWStatus, csw->bCSWStatus ? "FAILED" : "OK");
	return csw->bCSWStatus;
}


void command_init(CBW *cbw) {
	static uint32_t tag = 0;

	memset(cbw, 0, sizeof(CBW));
	memcpy(cbw->dCBWSignature, "USBC", 4);
	cbw->dCBWTag = ++tag;
	cbw->bCBWCBLength = 11;				// size of SCSI command
	cbw->mCBWFlags = LIBUSB_ENDPOINT_IN;
}


int command_perform_generic_read(CBW *cbw, USB_BULK_CONTEXT *uctx, unsigned char *data) {
	enum libusb_error usb_error = 0;
	CSW csw;
	int transferred;

//	libusb_reset_device(uctx->handle);
//	libusb_clear_halt(uctx->handle, uctx->endpoint_out);
//	libusb_clear_halt(uctx->handle, uctx->endpoint_in);

	dbg_printf("Start command 0x%02hhX - tag: %u\n", cbw->CBWCB[0], cbw->dCBWTag);

	// send CBW
	usb_error = libusb_bulk_transfer(uctx->handle, uctx->endpoint_out, (unsigned char *)cbw, sizeof(CBW), &transferred, USB_TIMEOUT);
	if (usb_error) {
		dbg_printf("USB error at bulk out (CBW): %s\n", libusb_strerror(usb_error));
		return usb_error;
	} else if (transferred != sizeof(CBW)) {
		dbg_printf("Warning - not all data transferred at out endpoint, %i instead of %i\n", transferred, sizeof(CBW));
	}

	// recieve requested data if present
	if (cbw->dCBWDataTransferLength > 0) {
		usb_error = libusb_bulk_transfer(uctx->handle, uctx->endpoint_in, data, cbw->dCBWDataTransferLength, &transferred, USB_TIMEOUT);
		if (usb_error) {
			dbg_printf("USB error at bulk in (data): %s\n", libusb_strerror(usb_error));
			return usb_error;
		} else if (transferred != cbw->dCBWDataTransferLength) {
			dbg_printf("Warning - not all data transferred at in endpoint, %i instead of %i\n", transferred, cbw->dCBWDataTransferLength);

			// we get unexpedted CSW
			if (transferred == 13 && is_csw(data)) {
				int err = 0;
				dbg_printf("Unexpected CSW - checking.\n");
				err = check_csw(cbw, (CSW *)data);
				if (err) {
					// clear endpoint if error
					libusb_clear_halt(uctx->handle, uctx->endpoint_in);
					return err;
				} else {
					// neverthles we get correct CSW we have no data so return error
					return -1;
				}
			}
		}
	}

	// recieve CSW
	usb_error = libusb_bulk_transfer(uctx->handle, uctx->endpoint_in, (unsigned char *)&csw, sizeof(CSW), &transferred, USB_TIMEOUT);
	if (usb_error) {
		dbg_printf("USB error at bulk in (CSW): %s\n", libusb_strerror(usb_error));
		return usb_error;
	} else if (transferred != sizeof(CSW)) {
		dbg_printf("Warning - not all data transferred at in endpoint, %i instead of %i\n", transferred, sizeof(CSW));
	}

	// return what CSW check do
	return check_csw(cbw, &csw);
}



// SCSI INQURY command

void command_init_inquiry(CBW *cbw, uint8_t lun) {
	command_init(cbw);
	cbw->bCBWLUN = lun;
	cbw->dCBWDataTransferLength = sizeof(SCSI_INQUIRY);
	cbw->CBWCB[SCSI_PACKET_CMD] = SCSI_CMD_INQUIRY;
	cbw->CBWCB[SCSI_PACKET_LUN] = ((lun << 5) & 0xFF);
	cbw->CBWCB[4] = sizeof(SCSI_INQUIRY);
}

int command_perform_inquiry(CBW *cbw, USB_BULK_CONTEXT *uctx, SCSI_INQUIRY *inquiry) {
	return command_perform_generic_read(cbw, uctx, (unsigned char *)inquiry);
}


// SCSI READ CAPACITY command

void command_init_read_capacity(CBW *cbw, uint8_t lun) {
	command_init(cbw);
	cbw->bCBWLUN = lun;
	cbw->dCBWDataTransferLength = sizeof(SCSI_CAPACITY);
	cbw->CBWCB[SCSI_PACKET_CMD] = SCSI_CMD_READ_CAPACITY;
	cbw->CBWCB[SCSI_PACKET_LUN] = ((lun << 5) & 0xFF);
}

int command_perform_read_capacity(CBW *cbw, USB_BULK_CONTEXT *uctx, SCSI_CAPACITY *capacity) {
	int err = command_perform_generic_read(cbw, uctx, (unsigned char *)capacity);

	if (err) {
		return err;
	}
	// response is in big endian
	capacity->lastLBA = be32toh(capacity->lastLBA);
	capacity->blockSize = be32toh(capacity->blockSize);

	return 0;
}

// SCSI READ10 (one sector) command

void command_init_read10one(CBW *cbw, uint8_t lun, uint32_t lba, uint32_t sector_size) {
	command_init(cbw);
	cbw->bCBWLUN = 0;
	cbw->dCBWDataTransferLength = sector_size;
	cbw->CBWCB[SCSI_PACKET_CMD] = SCSI_CMD_READ10;
	cbw->CBWCB[SCSI_PACKET_LUN] = lun;
	// big endian
	cbw->CBWCB[SCSI_PACKET_LBA + 0] = (lba >> 24) & 0xFF;
	cbw->CBWCB[SCSI_PACKET_LBA + 1] = (lba >> 16) & 0xFF;
	cbw->CBWCB[SCSI_PACKET_LBA + 2] = (lba >>  8) & 0xFF;
	cbw->CBWCB[SCSI_PACKET_LBA + 3] = (lba >>  0) & 0xFF;
	cbw->CBWCB[SCSI_PACKET_LENGTH + 0] = 0;
	cbw->CBWCB[SCSI_PACKET_LENGTH + 1] = 1;
}

int command_perform_read10one(CBW *cbw, USB_BULK_CONTEXT *uctx, uint8_t *buf) {
	return command_perform_generic_read(cbw, uctx, (unsigned char *)buf);
}




// ACTIONS INDENTIFY command

void command_init_act_identify(CBW *cbw, uint8_t lun) {
	command_init(cbw);
	cbw->bCBWLUN = lun;
	cbw->dCBWDataTransferLength = sizeof(ACTIONSUSBD);
	cbw->CBWCB[SCSI_PACKET_CMD] = SCSI_CMD_ACT_IDENTIFY;
}

int command_perform_act_identify(CBW *cbw, USB_BULK_CONTEXT *uctx, ACTIONSUSBD *actid) {
	return command_perform_generic_read(cbw, uctx, (unsigned char *)actid);
}

// ACTIONS INIT command

void command_init_act_init(CBW *cbw) {
	command_init(cbw);
	cbw->bCBWLUN = 0;
	cbw->dCBWDataTransferLength = sizeof(uint8_t);
	cbw->CBWCB[SCSI_PACKET_CMD] = SCSI_CMD_ACT_INIT;
}

int command_perform_act_init(CBW *cbw, USB_BULK_CONTEXT *uctx, uint8_t *buf) {
	return command_perform_generic_read(cbw, uctx, (unsigned char *)buf);
}

// ACTIONS DETACH command

void command_init_act_detach(CBW *cbw) {
	command_init(cbw);
	cbw->bCBWLUN = 0;
	cbw->dCBWDataTransferLength = 0;
	cbw->CBWCB[SCSI_PACKET_CMD] = SCSI_CMD_ACTF_DETACH;
}

int command_perform_act_detach(CBW *cbw, USB_BULK_CONTEXT *uctx) {
	return command_perform_generic_read(cbw, uctx, NULL);
}

// ACTIONS READ (one sector) command

void command_init_act_readone(CBW *cbw, uint8_t lun, uint32_t lba, bool is_log) {
	command_init(cbw);
	cbw->bCBWLUN = lun;
	cbw->dCBWDataTransferLength = SECTOR_SIZE;
	cbw->CBWCB[SCSI_PACKET_CMD] = is_log ? SCSI_CMD_ACTF_NAND_LOG : SCSI_CMD_ACTF_NAND_PHY;
	// at 'lun' offset place read mark
	cbw->CBWCB[SCSI_PACKET_LUN] = 0x80;
	// actions use little endian in diffrence of regular SCSI commands
	cbw->CBWCB[SCSI_PACKET_LBA + 0] = (lba >>  0) & 0xFF;
	cbw->CBWCB[SCSI_PACKET_LBA + 1] = (lba >>  8) & 0xFF;
	cbw->CBWCB[SCSI_PACKET_LBA + 2] = (lba >> 16) & 0xFF;
	cbw->CBWCB[SCSI_PACKET_LBA + 3] = (lba >> 24) & 0xFF;
	// so in transfer length
	cbw->CBWCB[SCSI_PACKET_LENGTH + 0] = 1;
	cbw->CBWCB[SCSI_PACKET_LENGTH + 1] = 0;
}

int command_perform_act_readone(CBW *cbw, USB_BULK_CONTEXT *uctx, uint8_t *buf) {
	return command_perform_generic_read(cbw, uctx, (unsigned char *)buf);
}

// ACTIONS READ RAM command

void command_init_act_read_ram(CBW *cbw, uint16_t sector, uint16_t length) {
	//max sector is 0x800 and max size 0x200
	command_init(cbw);
	cbw->bCBWLUN = 0;
	cbw->dCBWDataTransferLength = length;
	cbw->CBWCB[SCSI_PACKET_CMD] = SCSI_CMD_ACTF_RAM;
	// at 'lun' offset place read mark
	cbw->CBWCB[SCSI_PACKET_LUN] = 0x80;
	// little endian sectorn number in LBA
	cbw->CBWCB[SCSI_PACKET_LBA + 0] = (sector >> 0) & 0xFF;
	cbw->CBWCB[SCSI_PACKET_LBA + 1] = (sector >> 8) & 0xFF;
	cbw->CBWCB[SCSI_PACKET_LBA + 2] = 0;
	cbw->CBWCB[SCSI_PACKET_LBA + 3] = 0;
	cbw->CBWCB[SCSI_PACKET_LENGTH + 0] = (length >> 0) & 0xFF;
	cbw->CBWCB[SCSI_PACKET_LENGTH + 1] = (length >> 8) & 0xFF;
}

int command_perform_act_read_ram(CBW *cbw, USB_BULK_CONTEXT *uctx, uint8_t *buf) {
	return command_perform_generic_read(cbw, uctx, (unsigned char *)buf);
}



