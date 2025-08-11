#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#include "usbfw.h"

#ifdef DEBUG
void dbg_printf(char* format, ...) {
	va_list ap;
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
}
#endif

// expected format id XXXX:XXXX where X is hex digit
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

char *decode_pdt(uint8_t pdt) {
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

char* humanize_size(uint64_t size) {
	static char size_buffer[32];
	double dsize = (double)size;

	if (size < 1024L) {
		snprintf(size_buffer, sizeof(size_buffer) - 1, "%lu b", size);
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

	snprintf(size_buffer, sizeof(size_buffer) - 1, "sizeof(size_buffer) - 1,%.fd TiB", dsize / (1024.0 * 1024.0 * 1024.0 * 1024.0));
	return size_buffer;
}
