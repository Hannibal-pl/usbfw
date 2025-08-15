#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <libgen.h>

#pragma pack(1)

typedef struct {
	uint8_t			file_type;		// 'P'
	uint8_t			ap_type;		// 0:ap_system, 1:ap_user
	char			magic[4];		// 0x57, 0x47, 0x19, 0x97
	uint8_t			major_version;
	uint8_t			minor_version;

	uint32_t		text_offset;
	uint16_t		text_length;
	uint16_t		text_addr;

	uint32_t		data_offset;
	uint16_t		data_length;
	uint16_t		data_addr;

	uint16_t		bss_length;
	uint16_t		bss_addr;

	uint16_t		entry;
	uint8_t			entry_bank;
	uint8_t			banks;			// max 252
} AP_HEAD;


typedef struct {
	uint32_t		offset;
	uint16_t		length;
	uint16_t		addr;
} AP_BANK;


typedef struct {
	AP_HEAD			head;
	AP_BANK			bank[252];
} AP_FULLHEAD;

#pragma pack()

char magic[4] = { 0x57, 0x47, 0x19, 0x97 };

FILE *apfile;
AP_FULLHEAD ap_header;


void usage(char *app) {
	printf("Usage:\n\t%s AP_FILE\n\n", app);
}

char * decode_ap_type(uint8_t ap_type) {
	switch (ap_type) {
		case 0:
			return "system";
		case 1:
			return "user";
		default:
			return "unknown";
	}
}

int main(int argc, char *argv[]) {
	bool is_bank = false;

	if (argc < 2) {
		usage(basename(argv[0]));
		return -1;
	}

	apfile = fopen(argv[1], "r");
	if (!apfile) {
		printf("Error: Cannot open file \"%s\".\n\n", argv[1]);
		return 0;
	}

	fread(&ap_header, sizeof(AP_FULLHEAD), 1, apfile);

	if ((ap_header.head.file_type != 'P') && memcmp(ap_header.head.magic, magic, 4)) {
		printf("Error: File \"%s\" isn't proper ACTOS application.\n\n", argv[1]);
	}

	printf("\n    HEADER INFORMATION:\n");
	printf("    -------------------\n\n");
	printf("             File Type : %c\n", ap_header.head.file_type);
	printf("      Application Type : 0x%02hhX (%s)\n", ap_header.head.ap_type, decode_ap_type(ap_header.head.ap_type));
	printf("         ACTOS Version : %hhu.%hhu\n", ap_header.head.major_version, ap_header.head.minor_version);
	printf("   TEXT Segment Offset : 0x%08X\n", ap_header.head.text_offset);
	printf("                Length : 0x%04hX\n", ap_header.head.text_length);
	printf("               Address : 0x%04hX\n", ap_header.head.text_addr);
	printf("   DATA Segment Offset : 0x%08X\n", ap_header.head.data_offset);
	printf("                Length : 0x%04hX\n", ap_header.head.data_length);
	printf("               Address : 0x%04hX\n", ap_header.head.data_addr);
	printf("    BSS Segment Length : 0x%04hX\n", ap_header.head.bss_length);
	printf("               Address : 0x%04hX\n", ap_header.head.bss_addr);
	printf("                 Entry : 0x%04hX\n", ap_header.head.entry);
	printf("            Entry Bank : 0x%02hhX\n", ap_header.head.entry_bank);
	printf("                 Banks : 0x%02hhX\n", ap_header.head.banks);

	printf("\n\n    NON EMPTY BANKS:\n");
	printf("    ----------------\n\n");

	for (uint32_t i = 0; i < 252; i++) {
		if (ap_header.bank[i].offset != 0) {
			is_bank = true;
			printf("      Bank 0x%02hhX Offset : 0x%08X\n", i, ap_header.bank[i].offset);
			printf("                Lenght : 0x%04hX\n", ap_header.bank[i].length);
			printf("               Address : 0x%04hX\n\n", ap_header.bank[i].addr);
		}
	}

	if (!is_bank) {
		printf("    None\n\n");
	}

	fclose(apfile);
	return 0;
}
