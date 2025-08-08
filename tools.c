#include <stdio.h>
#include <stdarg.h>

#include "usbfw.h"

#ifdef DEBUG
void dbg_printf(char* format, ...) {
	va_list ap;
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
}
#endif
