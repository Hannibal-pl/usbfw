#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <libgen.h>

#pragma pack(1)

#define		BANKA_SIZE		0x200
#define		BANKB_SIZE		0x600

typedef struct {
	uint8_t			file_type;		// 'D'
	uint8_t			drv_type;
	uint16_t		rcode_addr;
	uint16_t		rcode_length;
	uint16_t		init_API;
	uint16_t		exit_API;
	uint32_t		bankA_offset;
	uint32_t		bankB_offset;
} DRV_HEAD;

typedef struct {
	uint16_t		length;
	uint16_t		entry_points[8];
	uint8_t			local_var_length;
} BANK_HEAD;

#pragma pack()

char magic[4] = { 0x57, 0x47, 0x19, 0x97 };

FILE *drvfile;
DRV_HEAD drv_header;
BANK_HEAD bank_header;


void usage(char *app) {
	printf("Usage:\n\t%s DRV_FILE\n\n", app);
}

char * decode_drv_type(uint8_t ap_type) {
	switch (ap_type) {
		case 0:
			return "virtual memory";
		case 1:
			return "user memory";
		case 2:
			return "keyboard";
		case 3:
			return "display";
		case 4:
			return "filesystem";
		case 5:
			return "SD card fast";
		case 6:
			return "I2C";
		case 7:
			return "SDRAM";
		default:
			return "unknown";
	}
}

int main(int argc, char *argv[]) {
	bool is_bank;

	if (argc < 2) {
		usage(basename(argv[0]));
		return -1;
	}

	drvfile = fopen(argv[1], "r");
	if (!drvfile) {
		printf("Error: Cannot open file \"%s\".\n\n", argv[1]);
		return 0;
	}

	fread(&drv_header, sizeof(DRV_HEAD), 1, drvfile);

	if (drv_header.file_type != 'D') {
		printf("Error: File \"%s\" isn't proper ACTOS driver.\n\n", argv[1]);
	}

	printf("\n    HEADER INFORMATION:\n");
	printf("    -------------------\n\n");
	printf("             File Type : %c\n", drv_header.file_type);
	printf("           Driver Type : 0x%02hhX (%s)\n", drv_header.drv_type, decode_drv_type(drv_header.drv_type));
	printf(" Resident Code Address : 0x%04hX\n", drv_header.rcode_addr);
	printf("                Length : 0x%04hX\n", drv_header.rcode_length);
	printf("     Init Code Address : 0x%04hX\n", drv_header.init_API);
	printf("     Exit Code Address : 0x%04hX\n", drv_header.exit_API);
	printf("         Bank A Offset : 0x%08X\n", drv_header.bankA_offset);
	printf("         Bank B Offset : 0x%08X\n", drv_header.bankB_offset);

	printf("\n\n    'A' BANKS:\n");
	printf("    ----------\n\n");

	is_bank = false;
	uint32_t offset = drv_header.bankA_offset;
	uint32_t end = drv_header.bankB_offset;

	for (uint32_t i = 0; offset < end; i++, offset += BANKA_SIZE) {
		is_bank = true;
		fseek(drvfile, offset, SEEK_SET);
		fread(&bank_header, sizeof(BANK_HEAD), 1, drvfile);

		if (bank_header.length < sizeof(BANK_HEAD)) {
			printf("    Bank A 0x%02hhX Length : 0x%04hX (Nonstandard format)\n\n", i, bank_header.length);
			continue;
		} else if (bank_header.length == 0x7676) {
			printf("    Bank A 0x%02hhX Length : Empty \n\n", i);
			continue;
		}

		printf("    Bank A 0x%02hhX Length : 0x%04hX\n", i, bank_header.length);
		printf("     Local Vars Length : 0x%02hhX\n", bank_header.local_var_length);
		for (uint32_t j = 0; j < 8; j++) {
			if (bank_header.entry_points[j] != 0x7676) {
				printf("         %s %1u : 0x%04hX\n", j ? "           " : "Entry point", j, bank_header.entry_points[j]);
			}
		}
		printf("\n");
	}

	if (!is_bank) {
		printf("    None\n\n");
	}


	printf("\n\n     'B' BANKS:\n");
	printf("     ----------\n\n");

	is_bank = false;
	offset = drv_header.bankB_offset;
	fseek(drvfile, 0, SEEK_END);
	end = ftell(drvfile);

	for (uint32_t i = 0; offset < end; i++, offset += BANKB_SIZE) {
		is_bank = true;
		fseek(drvfile, offset, SEEK_SET);
		fread(&bank_header, sizeof(BANK_HEAD), 1, drvfile);

		if (bank_header.length < sizeof(BANK_HEAD)) {
			printf("    Bank B 0x%02hhX Length : 0x%04hX (Nonstandard format)\n\n", i, bank_header.length);
			continue;
		} else if (bank_header.length == 0x7676) {
			printf("    Bank A 0x%02hhX Length : Empty \n\n", i);
			continue;
		}

		printf("    Bank B 0x%02hhX Length : 0x%04hX\n", i, bank_header.length);
		printf("     Local Vars Length : 0x%02hhX\n", bank_header.local_var_length);
		for (uint32_t j = 0; j < 8; j++) {
			if (bank_header.entry_points[j] != 0x7676) {
				printf("         %s %1u : 0x%04hX\n", j ? "           " : "Entry point", j, bank_header.entry_points[j]);
			}
		}
		printf("\n");
	}

	if (!is_bank) {
		printf("    None\n\n");
	}

	fclose(drvfile);
	return 0;
}
