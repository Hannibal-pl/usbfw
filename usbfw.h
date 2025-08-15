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
#define		SCSI_CMD_ACTF_ENTRY	0x20
#define		SCSI_CMD_READ_FCAPACITY	0x23
#define		SCSI_CMD_READ_CAPACITY	0x25
#define		SCSI_CMD_READ10		0x28
#define		SCSI_CMD_WRITE10	0x2A
#define		SCSI_CMD_ACT_INIT	0xCB
#define		SCSI_CMD_ACT_IDENTIFY	0xCC

// common SCSI command packet offsets
#define		SCSI_PACKET_CMD		0
#define		SCSI_PACKET_LUN		1		// top 3 bits
#define		SCSI_PACKET_LBA		2
#define		SCSI_PACKET_LENGTH	7

// AFI download address
#define		AFI_DADDR_B		0x00000006
#define		AFI_DADDR_I		0x00000011

// colors
#define		COLOR_DEFAULT		"\033[0m"
#define		COLOR_RED		"\033[31m"
#define		COLOR_GREEN		"\033[32m"

// long commands
#define		CMDLINE_YESIKNOW	1000

// other
#define		USB_TIMEOUT		1000		// 1s
#define		SECTOR_SIZE		512
#define		SYSINFO_SIZE		192
#define		DEFAULT_OUT_FILENAME	"read_out.bin"
#define		DEFAULT_IN_FILENAME	"write_in.bin"
#define		MAX_SEARCH_LBA		65535		// max sector for alternate firmware search

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
	APPCMD_FCAPACITY,
	APPCMD_CAPACITY,
	APPCMD_HEADINFO,
	APPCMD_SYSINFO,
	APPCMD_READ,
	APPCMD_WRITE,
	APPCMD_READ_FW,
	APPCMD_TEST_RAMACC,
	APPCMD_READ_RAM,
	APPCMD_DUMP_RAW,
	APPCMD_DUMP_AFI,
	APPCMD_ENTRY
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
	char				*ofilename;	// output filename
	FILE				*ofile;		// output file
	char				*ifilename;	// input filename
	FILE				*ifile;		// input file
	uint32_t			offset;		// input file offset
	bool				is_dev;		// vendor and product ID are set
	uint16_t			vid;		// vendor ID
	uint16_t			pid;		// product ID
	uint8_t				lun;		// logical device number
	uint32_t			lba;		// logical block number
	uint32_t			bc;		// block count
	bool				is_logical;	// logical or phisical fw sectors
	bool				is_showdir;	// show directory in APPCMD_HEADINFO
	bool				is_detach;	// detach device at exit
	bool				is_alt_fw;	// use alternate (backup?) firmware
	bool				is_yesiknow;	// confirmation of dangerous commands
	uint16_t			entry_param;	// parameter for entry command
} APP_CONTEXT;


//afi.c
FILE * afi_new_file(char *filename);
void afi_add_whole(FILE *fafi, FW_AFI_DIR_ENTRY *afi_entry, uint8_t* data);
void afi_add_appended(FILE *fafi, FW_AFI_DIR_ENTRY *afi_entry);

//cmdline.c
void parseparams(int argc, char *argv[]);

//commands.c
void command_init(CBW *cbw);

void command_init_inquiry(CBW *cbw, uint8_t lun);
int command_perform_inquiry(CBW *cbw, USB_BULK_CONTEXT *uctx, SCSI_INQUIRY *inquiry);
void command_init_read_fcapacity(CBW *cbw, uint8_t lun);
int command_perform_read_fcapacity(CBW *cbw, USB_BULK_CONTEXT *uctx, SCSI_FORMAT_CAPACITY *fcapacity);
void command_init_read_capacity(CBW *cbw, uint8_t lun);
int command_perform_read_capacity(CBW *cbw, USB_BULK_CONTEXT *uctx, SCSI_CAPACITY *capacity);
void command_init_read10one(CBW *cbw, uint8_t lun, uint32_t lba, uint32_t sector_size);
int command_perform_read10one(CBW *cbw, USB_BULK_CONTEXT *uctx, uint8_t *buf);
void command_init_write10one(CBW *cbw, uint8_t lun, uint32_t lba, uint32_t sector_size);
int command_perform_write10one(CBW *cbw, USB_BULK_CONTEXT *uctx, uint8_t *buf);

void command_init_act_identify(CBW *cbw, uint8_t lun);
int command_perform_act_identify(CBW *cbw, USB_BULK_CONTEXT *uctx, ACTIONSUSBD *actid);
void command_init_act_init(CBW *cbw);
int command_perform_act_init(CBW *cbw, USB_BULK_CONTEXT *uctx, uint8_t *buf);
void command_init_act_detach(CBW *cbw);
int command_perform_act_detach(CBW *cbw, USB_BULK_CONTEXT *uctx);
void command_init_act_readone(CBW *cbw, uint8_t lun, uint32_t lba, bool is_log);
int command_perform_act_readone(CBW *cbw, USB_BULK_CONTEXT *uctx, uint8_t *buf);
void command_init_act_read_ram(CBW *cbw, uint16_t sector, uint16_t length);
int command_perform_act_read_ram(CBW *cbw, USB_BULK_CONTEXT *uctx, uint8_t *buf);
void command_init_act_entry(CBW *cbw, uint16_t param);
int command_perform_act_entry(CBW *cbw, USB_BULK_CONTEXT *uctx);

//context.c
void zero_bulk_context(USB_BULK_CONTEXT *uctx);
int init_bulk_context(USB_BULK_CONTEXT *uctx, libusb_device *dev);
int claim_bulk_context(USB_BULK_CONTEXT *uctx);
void free_bulk_context(USB_BULK_CONTEXT *uctx);
bool open_device(USB_BULK_CONTEXT *uctx, uint16_t vid, uint16_t pid);
bool open_and_claim(USB_BULK_CONTEXT *uctx, uint16_t vid, uint16_t pid);

//fw.c
bool init_act(USB_BULK_CONTEXT *uctx);
uint32_t search_alternate_fw(USB_BULK_CONTEXT *uctx, uint8_t lun, uint32_t max_lba);
bool get_fw_header(USB_BULK_CONTEXT *uctx, FW_HEADER *fw_header, uint8_t lun, uint32_t start_lba);
uint32_t get_fw_size(USB_BULK_CONTEXT *uctx, uint8_t lun, uint32_t start_lba);
bool test_ram_access(USB_BULK_CONTEXT *uctx);
bool get_fw_sysinfo(USB_BULK_CONTEXT *uctx, FW_SYSINFO *sysinfo);
void detach_device(USB_BULK_CONTEXT *uctx, bool detach);

//main.c
extern APP_CONTEXT app;

//tool.c
bool parse_devid(char *devstring);
char * decode_pdt(uint8_t);
char * decode_fcapacity(uint8_t desc);
char * humanize_size(uint64_t size);
char * covert_usb_string_descriptor(uint8_t *src, uint32_t length);
char * convert_mtp_serial(uint8_t serial[16]);
char * decode_langid(uint8_t langid);
char * decode_battery(uint8_t battery);
char * make_filename(char filename[11]);
void display_spinner(void);
void display_percent_spinner(uint32_t current, uint32_t max);
bool test_ram_access(USB_BULK_CONTEXT *uctx);
bool confirm(void);
uint16_t checksum16(uint16_t *data, uint32_t size, bool is_new);
uint32_t checksum32(uint32_t *data, uint32_t size, bool is_new);

#endif
