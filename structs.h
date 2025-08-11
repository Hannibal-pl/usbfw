#ifndef __USBFW_STRUCTS_H__
#define __USBFW_STRUCTS_H__

#ifndef __USBFW_H__
#error "Don't include this file directly, include 'usbfw.h' instead"
#endif

#pragma pack(1)


typedef struct { // USB command block wrapper
	uint8_t			dCBWSignature[4];
	uint32_t		dCBWTag;
	uint32_t		dCBWDataTransferLength;
	uint8_t			mCBWFlags;
	uint8_t			bCBWLUN;
	uint8_t			bCBWCBLength;
	uint8_t			CBWCB[16];		// SCSI packet command
} CBW;


typedef struct { // USB command status wrapper
	uint8_t			dCSWSignature[4];
	uint32_t		dCSWTag;
	uint32_t		dCSWDataResidue;
	uint8_t			bCSWStatus;
} CSW;

// standard SCSI commands

typedef struct {
	uint8_t			reserved1:3;
	uint8_t			pdt:5;			// Device type
//1
	bool			is_removable:1;
	uint8_t			reserved2:7;
//2
	uint8_t			iso_version:2;
	uint8_t			ecma_version:3;
	uint8_t			ansi_version:3;
//3
	uint8_t			reserved3:5;
	uint8_t			rdt:3;			// Response type
//4
	uint8_t			additionalLength;
//5-7
	uint8_t			reserved4[3];
//8-15
	char			vendor[8];
//16-31
	char			product[16];
//32-35
	char			rev[4];
} SCSI_INQUIRY;

typedef struct {
	uint32_t		lastLBA;		// Big endian
	uint32_t		blockSize;		// Big endian
} SCSI_CAPACITY;


// Actions specfic commands

typedef struct {
	char			actionsusbd[11];
	uint8_t			adfu;
	uint8_t			unknown;
} ACTIONSUSBD;



// Fimware structs

typedef struct {
	char			filename[11];		// 8.3
	uint8_t			attr;
	uint8_t			reserved1[2];
	uint16_t		version;
	uint32_t		offset;			// In sectors
	uint32_t		length;			// In bytes
	uint8_t			reserved2[4];
	uint32_t		checksum;
} FW_DIR_ENTRY;


typedef struct {
	uint16_t		magic;			// 0xDEAD
	uint32_t		systemtime;		// in 0.5s
	uint16_t		RTCRate;		// 950
	uint8_t			displayContrast;	// 0~31
	uint8_t			lightTime;		// default is 5s (0x0A)
	uint8_t			standbyTime;
	uint8_t			sleepTime;
	uint8_t			langid;			// interface language, Simplified: 0, English: 1, Traditional: 2
	uint8_t			replayMode;
	uint8_t			onlineMode;
	uint8_t			batteryType;
	uint8_t			fmBuildIn;
} FW_COMMON_VAL;

typedef struct {
	uint32_t 		magic;			// 0x0FF0AA55
	uint8_t			version[4];		// x.x.xx.xxxx
	uint8_t			date[4];		// xxxx.xx.xx
	uint16_t 		vendorId;		// vendor ID and product ID are exchange
	uint16_t		productId;		// in original header
	uint32_t		dirCheckSum;		// for all 240 catalog items combined
	uint8_t			reserved1[12];		// fwDescriptor is shorter than in
	uint8_t			fwDescriptor[32];	// original header
	uint8_t			producer[32];
	uint8_t			deviceName[32];
	uint8_t			reserved2[128];
	uint8_t			usbAttri[8];
	uint8_t			usbIdentification[16];
	uint8_t			usbProductVer[4];
	uint8_t			reserved3[4];
	uint8_t			bLength;		// 48
	uint8_t			bDescriptorType;	// 3 (string)
	uint8_t			bString[46];
	FW_COMMON_VAL		defaultInf;
	uint8_t			reserved4[15];
	uint8_t			asciiLen1;
	uint8_t			mtpManufacturerInfo[32];
	uint8_t			asciilen2;
	uint8_t			mtpProductInfo[32];
	uint8_t			asciilen3;
	uint8_t			mtpProductVersion[16];
	uint8_t			asciilen4;
	uint8_t			mtpProductSerialNumber[16];
	uint16_t		mtpVendorId;		// in original header those two are 2 element arrays
	uint16_t		mtpProductId;		// but this make this structure too large. And in MTP
	uint8_t			reserved5[38];		// they are 16 bit wide
	uint16_t		headerChecksum;		// first 510 bytes
	FW_DIR_ENTRY		diritem[240];
} FW_HEADER;

#pragma pack()

#endif
