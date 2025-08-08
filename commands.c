#include <string.h>

#include "usbfw.h"


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
		dbg_printf("USB error at bulk out (CBW): %s\n",libusb_strerror(usb_error));
		return usb_error;
	} else if (transferred != sizeof(CBW)) {
		dbg_printf("Warning - not all data transferred at out endpoint, %i instead of %i\n",transferred, sizeof(CBW));
	}

	// recieve requested data
	usb_error = libusb_bulk_transfer(uctx->handle, uctx->endpoint_in, data, cbw->dCBWDataTransferLength, &transferred, USB_TIMEOUT);
	if (usb_error) {
		dbg_printf("USB error at bulk in (data): %s\n", libusb_strerror(usb_error));
		return usb_error;
	} else if (transferred != cbw->dCBWDataTransferLength) {
		dbg_printf("Warning - not all data transferred at in endpoint, %i instead of %i\n", transferred, cbw->dCBWDataTransferLength);
	}

	// recieve CSW
	usb_error = libusb_bulk_transfer(uctx->handle, uctx->endpoint_in, (unsigned char *)&csw, sizeof(CSW), &transferred, USB_TIMEOUT);
	if (usb_error) {
		dbg_printf("USB error at bulk in (CSW): %s\n", libusb_strerror(usb_error));
		return usb_error;
	} else if (transferred != sizeof(CSW)) {
		dbg_printf("Warning - not all data transferred at in endpoint, %i instead of %i\n", transferred, sizeof(CSW));
	}


	// check CSW
	if (strncmp((char *)csw.dCSWSignature, "USBS", 4)) {
		dbg_printf("CSW signature not present '%c%c%c%c' != 'USBS'\n", csw.dCSWSignature[0], csw.dCSWSignature[1], csw.dCSWSignature[2], csw.dCSWSignature[3]);
		return -1;
	}

	// Tag differs, return error
	if (csw.dCSWTag != cbw->dCBWTag) {
		dbg_printf("CSW tag mismatch %u != %u\n", csw.dCSWTag, cbw->dCBWTag);
		return -1;
	}

	dbg_printf("CSW Status 0x%02hhX - %s\n", csw.bCSWStatus, csw.bCSWStatus ? "FAILED" : "OK");
	return csw.bCSWStatus;
}



// SCSI INQURY command

void command_init_inquiry(CBW *cbw) {
	command_init(cbw);
	cbw->bCBWLUN = 0;
	cbw->dCBWDataTransferLength = sizeof(SCSI_INQUIRY);	// size of inquiry response
	cbw->CBWCB[SCSI_PACKET_CMD] = SCSI_CMD_INQUIRY;
	cbw->CBWCB[SCSI_PACKET_LUN] = 0;
	cbw->CBWCB[4] = sizeof(SCSI_INQUIRY);
}

int command_perform_inquiry(CBW *cbw, USB_BULK_CONTEXT *uctx, SCSI_INQUIRY *inquiry) {
	return command_perform_generic_read(cbw, uctx, (unsigned char *)inquiry);
}



// ACTION INDENTIFY command

void command_init_act_identify(CBW *cbw) {
	command_init(cbw);
	cbw->bCBWLUN = 0;
	cbw->dCBWDataTransferLength = sizeof(ACTIONSUSBD);
	cbw->CBWCB[SCSI_PACKET_CMD] = SCSI_CMD_ACT_IDENTIFY;
}

int command_perform_act_identify(CBW *cbw, USB_BULK_CONTEXT *uctx, ACTIONSUSBD *actid) {
	return command_perform_generic_read(cbw, uctx, (unsigned char *)actid);
}
