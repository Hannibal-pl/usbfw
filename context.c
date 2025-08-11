#include <string.h>

#include "usbfw.h"

void zero_bulk_context(USB_BULK_CONTEXT *uctx) {
	memset(&uctx->dev_descr, 0, sizeof(struct libusb_device_descriptor));
	uctx->conf_descr = NULL;
	uctx->handle = 0;
	uctx->interface = 0;
	uctx->is_claimed = false;
	uctx->endpoint_in = 0;
	uctx->endpoint_out = 0;
}

int init_bulk_context(USB_BULK_CONTEXT *uctx, libusb_device *dev) {
	enum libusb_error usb_error = 0;

	// clean structure
	zero_bulk_context(uctx);

	// get device descriptor
	usb_error = libusb_get_device_descriptor(dev, &uctx->dev_descr);
	if (usb_error) {
		printf("Error: Cannot get device descriptor: %s.\n", libusb_strerror(usb_error));
		free_bulk_context(uctx);
		return -1;
	}

	// open device
	usb_error = libusb_open(dev, &uctx->handle);
	if (usb_error) {
		printf("Error: Cannot open USB device: %s.\n", libusb_strerror(usb_error));
		free_bulk_context(uctx);
		return -1;
	}

	// get configuration descriptor
	usb_error = libusb_get_config_descriptor(dev, 0, &uctx->conf_descr);
	if (usb_error) {
		printf("Error: Cannot get configuration descriptor: %s.\n", libusb_strerror(usb_error));
		free_bulk_context(uctx);
		return -1;
	}

	// enumerate all interfaces for SFF-8070(ATAPI) or SCSI mass storeage bulk ones
	for (uint32_t i = 0; i < uctx->conf_descr->bNumInterfaces; i++) {
		uctx->interface = i;
		for (uint32_t j = 0; j < uctx->conf_descr->interface[i].num_altsetting; j++) {
			uint8_t class = uctx->conf_descr->interface[i].altsetting[j].bInterfaceClass;
			uint8_t subclass = uctx->conf_descr->interface[i].altsetting[j].bInterfaceSubClass;
			uint8_t protocol = uctx->conf_descr->interface[i].altsetting[j].bInterfaceProtocol;

			// class    == 8     -> mass storage
			// subclass == 5/6   -> SF-8070/SCSI
			// protocol == 80    -> BULK
			if ((class != 8) || ((subclass != 5) && (subclass != 6)) || (protocol != 80)) {
				continue;
			}

			dbg_printf("Found SFF-8070/SCSI mass storage device: %04X:%04X\n", uctx->dev_descr.idVendor, uctx->dev_descr.idProduct);

			// locate IN & OUT endpoints
			uint32_t endpoints_found = 0 ; // 0x1 - IN, 0x2 - OUT bitmask
			for (uint32_t k = 0; k < uctx->conf_descr->interface[i].altsetting[j].bNumEndpoints; k++) {
				const struct libusb_endpoint_descriptor *endpoint = &uctx->conf_descr->interface[i].altsetting[j].endpoint[k];

				// only bulk ones
				if ((endpoint->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_BULK) {
					if (endpoint->bEndpointAddress & LIBUSB_ENDPOINT_IN) {
						uctx->endpoint_in = endpoint->bEndpointAddress;
						endpoints_found |= 0x01;
					} else {
						uctx->endpoint_out = endpoint->bEndpointAddress;
						endpoints_found |= 0x02;
					}
				}

				// we found both endpoints
				if (endpoints_found == 0x03) {
					dbg_printf("Endpoints : IN:0x%02hhx; OUT:0x%02hhx\n", uctx->endpoint_in, uctx->endpoint_out);
					libusb_set_auto_detach_kernel_driver(uctx->handle, 1);
					return 0;
				}
			}
			dbg_printf("Warning: No suitable endpoints found, ignoring device.\n");
		}
	}
//	dbg_printf("Warning: No SFF-8070/SCSI mass storage interfaces found.\n");
	return -1;
}

int claim_bulk_context(USB_BULK_CONTEXT *uctx) {
	enum libusb_error usb_error = 0;

	usb_error = libusb_claim_interface(uctx->handle, uctx->interface);
	if (usb_error) {
		dbg_printf("USB error: %s\n", libusb_strerror(usb_error));
		return usb_error;
	}

	uctx->is_claimed = true;
	dbg_printf("Interface claimed.\n");

	return 0;
}

void free_bulk_context(USB_BULK_CONTEXT *uctx) {
	// first relese interface ...
	if (uctx->is_claimed) {
		libusb_release_interface(uctx->handle, uctx->interface);
		uctx->interface = 0;
		uctx->is_claimed = false;
	}

	// ... next zero endpoints, ...
	uctx->endpoint_in = 0;
	uctx->endpoint_out = 0;

	// ... free config descriptor, ...
	if (uctx->conf_descr) {
		libusb_free_config_descriptor(uctx->conf_descr);
		uctx->conf_descr = NULL;
	}

	// ... close device
	if (uctx->handle) {
		libusb_close(uctx->handle);
		uctx->handle = 0;
	}

	// ... and finally zero device descriptor
	memset(&uctx->dev_descr, 0, sizeof(struct libusb_device_descriptor));
}

bool open_device(USB_BULK_CONTEXT *uctx, uint16_t vid, uint16_t pid) {
	libusb_device **list;
	ssize_t cnt;

	if ((vid == 0) && (pid == 0)) {
		printf("Error: You must provice real device id not 0000:0000.\n");
		return false;
	}

	cnt = libusb_get_device_list(NULL, &list);
	if (cnt < 0) {
		printf("Error: Cannot enumerate USB devices: %s\n", libusb_strerror((enum libusb_error)cnt));
		return false;
	}

	for (uint32_t i = 0; i < cnt; i++) {

		// device is not mass storage or error
		if (init_bulk_context(uctx, list[i])) {
			free_bulk_context(uctx);
			continue;
		}

		if ((uctx->dev_descr.idVendor == vid) && (uctx->dev_descr.idProduct == pid)) {
			libusb_free_device_list(list, 1);
			return true;
		}

	}

	libusb_free_device_list(list, 1);
	return false;
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
