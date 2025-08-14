#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <iconv.h>

#include "usbfw.h"

#ifdef DEBUG
void dbg_printf(char* format, ...) {
	va_list ap;
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
}
#endif

// expected format is XXXX:XXXX where X is hex digit
bool parse_devid(char *devstring) {
	char *product;
	uint32_t vid, pid;

	if (!devstring) {
		return false;
	}


	product = strchr(devstring, ':');
	if (!product) {
		return false;
	}

	vid = strtoul(devstring, NULL, 16);
	pid = strtoul(++product, NULL, 16);

	if ((vid > 0xFFFF) || (pid > 0xFFFF)) {
		return false;
	}

	app.vid = (uint16_t)(vid & 0xFFFF);
	app.pid = (uint16_t)(pid & 0XFFFF);
	app.is_dev = true;
	return true;
}

char * decode_pdt(uint8_t pdt) {
	switch (pdt) {
		case 0x00:
			return "Direct access device";
		case 0x01:
			return "Sequential access device";
		case 0x04:
			return "Write once device";
		case 0x05:
			return "CD-ROM device";
		case 0x07:
			return "Optical memory device";
		case 0x1F:
			return "No device type";
		default:
			return "Reserved";
	}
}

char * humanize_size(uint64_t size) {
	static char size_buffer[32];
	double dsize = (double)size;

	if (size < 1024L) {
		snprintf(size_buffer, sizeof(size_buffer) - 1, "%lu B", size);
		return size_buffer;
	}

	if (size < (1024L * 1024L)) {
		snprintf(size_buffer, sizeof(size_buffer) - 1, "%.3f kiB", dsize / 1024.0);
		return size_buffer;
	}

	if (size < (1024L * 1024L * 1024L)) {
		snprintf(size_buffer, sizeof(size_buffer) - 1, "%.3f MiB", dsize / (1024.0 * 1024.0));
		return size_buffer;
	}

	if (size < (1024L * 1024L * 1024L * 1024L)) {
		snprintf(size_buffer, sizeof(size_buffer) - 1, "%.3f GiB", dsize / (1024.0 * 1024.0 * 1024.0));
		return size_buffer;
	}

	snprintf(size_buffer, sizeof(size_buffer) - 1, "%.fd TiB", dsize / (1024.0 * 1024.0 * 1024.0 * 1024.0));
	return size_buffer;
}


char * covert_usb_string_descriptor(uint8_t *src, uint32_t length) {
	static char buf[256];
	char *bufoutptr = buf;
	char *bufinptr = (char *)src;
	size_t bufinlen = length;
	size_t bufoutlen = sizeof(buf);

	iconv_t utf16_to_utf8 = iconv_open("UTF-8", "UTF-16LE");
	iconv(utf16_to_utf8, &bufinptr, &bufinlen, &bufoutptr, &bufoutlen);
	iconv_close(utf16_to_utf8);

	return buf;
}

char * convert_mtp_serial(uint8_t serial[16]) {
	static char buf[33];
	char digit;

	for (uint32_t i = 0; i < 16; i++) {
		digit = ((serial[i] >> 4) & 0xF);
		if (digit <= 9) {
			digit += '0';
		} else {
			digit += 'A' - 10;
		}

		buf[2 * i + 0] = digit;

		digit = (serial[i] & 0xF);
		if (digit <= 9) {
			digit += '0';
		} else {
			digit += 'A' - 10;
		}

		buf[2 * i + 1] = digit;
	}
	buf[32] = '0';

	return buf;
}

char * decode_langid(uint8_t langid) {
	switch (langid) {
		case 0:
			return "Chinese Simplified";
		case 1:
			return "English";
		case 2:
			return "Chinese Traditional";
		default:
			return "Unknown Language";
	}
}

char * decode_battery(uint8_t battery) {
	switch (battery) {
		case 0:
			return "Alkaline";
		case 1:
			return "Ni/H";
		case 2:
			return "Lithium";
		default:
			return "Unknown Battery";
	}
}

char * make_filename(char filename[11]) {
	static char outname[13];
	uint32_t i = 0;

	memset(outname, ' ', sizeof(outname));
	for (uint32_t j = 0; j < 8; j++) {
		if (filename[j] == ' ') {
			break;
		}
		outname[i++] = filename[j];
	}

	outname[i++] = '.';

	for (uint32_t j = 8; j < 11; j++) {
		if (filename[j] == ' ') {
			break;
		}
		outname[i++] = filename[j];
	}

	outname[12] = 0;

	return outname;
}

void display_spinner(void) {
	char spinner[4] = "|/-\\";
	static int i = 0;

	printf("\b%c", spinner[i++]);
	fflush(stdout);
	if (i > 3) {
		i = 0;
	}
}

void display_percent_spinner(uint32_t current, uint32_t max) {
	char spinner[4] = "|/-\\";
	static int i = 0;
	uint32_t percent = 0;

	// avoid strange values
	if (current > max) {
		percent = 0;
	} else {
		percent = ((uint64_t)current) * 100 / max;
	}

	// place for digits
	printf("\b\b\b\b\b%2u%% %c", percent, spinner[i++]);
	fflush(stdout);
	if (i > 3) {
		i = 0;
	}
}


bool confirm(void) {
	if (!app.is_yesiknow) {
		printf("You have run "COLOR_RED"DANGEROUS"COLOR_DEFAULT" command!\nYou must confirm you action with adding param \"--yes-i-know-what-im-doing\".\n");
		return false;
	}

	return true;
}

//size is in bytes
uint16_t checksum16(uint16_t *data, uint32_t size, bool is_new) {
	static uint16_t checksum;

	if (size & 0x1) {
		dbg_printf("Checksum 16 data not aligned.\n");
	}

	size >>= 1;

	if (is_new) {
		checksum = 0;
	}

	for (uint32_t i = 0; i < size; i++) {
		checksum += data[i];
	}

	return checksum;
}

//size is in bytes
uint32_t checksum32(uint32_t *data, uint32_t size, bool is_new) {
	static uint32_t checksum;

	if (size & 0x3) {
		dbg_printf("Checksum 32 data not aligned.\n");
	}

	size >>= 2;

	if (is_new) {
		checksum = 0;
	}

	for (uint32_t i = 0; i < size; i++) {
		checksum += data[i];
	}

	return checksum;
}
