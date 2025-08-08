#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "usbfw.h"

USB_BULK_CONTEXT uctx;

int main (int argc, char *argv[]) {
	libusb_device **list;
	int err;
	ssize_t cnt;

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

		CBW test_cbw;

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
		command_init_act_identify(&test_cbw);
		err = command_perform_act_identify(&test_cbw, &uctx, &actid);
		if (!err) {
			printf("ACTIONSUSBD %s\n", (strncmp(actid.actionsusbd, "ACTIONSUSBD", 11)) ? "Missing" : "Present");
			printf("Gathered ID: %11s 0x%02hhx 0x%02hhx\n", actid.actionsusbd, actid.adfu, actid.unknown);
		}



next:
		free_bulk_context(&uctx);
	}

	libusb_free_device_list(list, 1);
	libusb_exit(NULL);
}
