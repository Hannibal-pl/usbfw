#ifndef __USBFW_H__
#define __USBFW_H__

#include <stdbool.h>
#include <stdio.h>
#include <libusb.h>

// SCSI commands
#define		SCSI_CMD_ACTF_RAM	0x05
#define		SCSI_CMD_ACTF_NAND_LOG	0x08
#define		SCSI_CMD_ACTF_NAND_PHY	0x09
#define		SCSI_CMD_INQUIRY	0x12
#define		SCSI_CMD_ACTF_DETACH	0x16
#define		SCSI_CMD_READ_CAPACITY	0x25
#define		SCSI_CMD_ACT_INIT	0xCB
#define		SCSI_CMD_ACT_IDENTIFY	0xCC

// common SCSI command packet offsets
#define		SCSI_PACKET_CMD		0
#define		SCSI_PACKET_LUN		1		// top 3 bits
#define		SCSI_PACKET_LBA		2
#define		SCSI_PACKET_LENGTH	7

// colors
#define		COLOR_DEFAULT		"\033[0m"
#define		COLOR_RED		"\033[31m"
#define		COLOR_GREEN		"\033[32m"

// other
#define		USB_TIMEOUT		1000		// 1s
#define		SECTOR_SIZE		512
#define		DEFAULT_OUT_FILENAME	"dump.bin"

#ifdef DEBUG
void dbg_printf(char* format, ...);
#else
#define		dbg_printf(a, ...)	{}
#endif

// USB & SCSI structs
#include "structs.h"

typedef enum {
	APPCMD_NONE = 0,
	APPCMD_ENUMERATE,
	APPCMD_INQUIRY,
	APPCMD_CAPACITY,
	APPCMD_HEADINFO,
	APPCMD_READ,
	APPCMD_READ_FW
} APP_COMMAND;


typedef struct {
	struct libusb_device_descriptor	dev_descr;
	struct libusb_config_descriptor	*conf_descr;
	libusb_device_handle		*handle;
	uint8_t				endpoint_in;
	uint8_t				endpoint_out;
	uint8_t				interface;
	bool				is_claimed;
} USB_BULK_CONTEXT;


typedef struct {
	APP_COMMAND			cmd;		// command to execute
	char				*filename;	// output filename
	FILE				*file;		// output file
	bool				is_dev;		// vendor and product ID are set
	uint16_t			vid;		// vendor ID
	uint16_t			pid;		// product ID
	uint8_t				lun;		// logical device number
	uint32_t			lba;		// logical block number
	uint32_t			bc;		// block count
	bool				is_logical;	// logical or phisical fw sectors
	bool				is_detach;	// detach device at exit
	bool				is_alt_fw;	// use alternate (backup?) firmware
} APP_CONTEXT;


//cmdline.c
void parseparams(int argc, char *argv[]);

//commands.c
void command_init(CBW *cbw);
void command_init_inquiry(CBW *cbw, uint8_t lun);
int command_perform_inquiry(CBW *cbw, USB_BULK_CONTEXT *uctx, SCSI_INQUIRY *inquiry);
void command_init_read_capacity(CBW *cbw, uint8_t lun);
int command_perform_read_capacity(CBW *cbw, USB_BULK_CONTEXT *uctx, SCSI_CAPACITY *capacity);
void command_init_act_identify(CBW *cbw, uint8_t lun);
int command_perform_act_identify(CBW *cbw, USB_BULK_CONTEXT *uctx, ACTIONSUSBD *actid);
void command_init_act_init(CBW *cbw);
int command_perform_act_init(CBW *cbw, USB_BULK_CONTEXT *uctx, uint8_t *buf);
void command_init_act_detach(CBW *cbw);
int command_perform_act_detach(CBW *cbw, USB_BULK_CONTEXT *uctx);
void command_init_act_readone(CBW *cbw, uint32_t lba, bool is_log);
int command_perform_act_readone(CBW *cbw, USB_BULK_CONTEXT *uctx, uint8_t *buf);
void command_init_act_read_ram(CBW *cbw, uint16_t sector, uint16_t length);
int command_perform_act_read_ram(CBW *cbw, USB_BULK_CONTEXT *uctx, uint8_t *buf);

//context.c
void zero_bulk_context(USB_BULK_CONTEXT *uctx);
int init_bulk_context(USB_BULK_CONTEXT *uctx, libusb_device *dev);
int claim_bulk_context(USB_BULK_CONTEXT *uctx);
void free_bulk_context(USB_BULK_CONTEXT *uctx);
bool open_device(USB_BULK_CONTEXT *uctx, uint16_t vid, uint16_t pid);

//main.c
extern APP_CONTEXT app;

//tool.c
bool parse_devid(char *devstring);
char *decode_pdt(uint8_t);
char* humanize_size(uint64_t size);

#endif
