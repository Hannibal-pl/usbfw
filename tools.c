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
