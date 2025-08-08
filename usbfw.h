#ifndef __USBFW_H__
#define __USBFW_H__

#include <stdbool.h>
#include <libusb.h>

// SCSI commands
#define		SCSI_CMD_INQUIRY	0x12
#define		SCSI_CMD_ACT_IDENTIFY	0xCC

// common SCSI command packet offsets
#define		SCSI_PACKET_CMD		0
#define		SCSI_PACKET_LUN		1		// top 3 bits
#define		SCSI_PACKET_LBA		2		// MSB first

// other
#define		USB_TIMEOUT		1000		// 1s

#ifdef DEBUG
void dbg_printf(char* format, ...);
#else
#define		dbg_printf(a, ...)	{}
#endif

// USB & SCSI structs
#include "structs.h"


typedef struct {
	struct libusb_device_descriptor	dev_descr;
	struct libusb_config_descriptor	*conf_descr;
	libusb_device_handle		*handle;
	uint8_t				endpoint_in;
	uint8_t				endpoint_out;
	uint8_t				interface;
	bool				is_claimed;
} USB_BULK_CONTEXT;


//context.c
void zero_bulk_context(USB_BULK_CONTEXT *uctx);
int init_bulk_context(USB_BULK_CONTEXT *uctx, libusb_device *dev);
int claim_bulk_context(USB_BULK_CONTEXT *uctx);
void free_bulk_context(USB_BULK_CONTEXT *uctx);

//commands.c
void command_init(CBW *cbw);
void command_init_inquiry(CBW *cbw);
int command_perform_inquiry(CBW *cbw, USB_BULK_CONTEXT *uctx, SCSI_INQUIRY *inquiry);
void command_init_act_identify(CBW *cbw);
int command_perform_act_identify(CBW *cbw, USB_BULK_CONTEXT *uctx, ACTIONSUSBD *actid);

#endif
