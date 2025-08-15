CC=gcc
#CFLAGS=-Wall -g -std=gnu99 -flto -DDEBUG
CFLAGS=-Wall -g -std=gnu99 -flto
INCLUDES=-I/usr/include -I/usr/include/libusb-1.0
LIBS=-L/usr/lib64 -lm -lusb-1.0
MOD=afi.o cmdline.o context.o commands.o fw.o main.o tools.o
TOOLS=$(patsubst %.c,%,$(foreach sdir,tools,$(wildcard $(sdir)/*.c)))


all: usbfw $(TOOLS)

$(MOD): %.o: %.c usbfw.h structs.h Makefile
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

usbfw: $(MOD)
	$(CC) $(CFLAGS) $(LIBS) -o usbfw $(MOD)

$(TOOLS): %: %.c Makefile
	$(CC) $(CFLAGS) $(LIBS) $< -o $@

clean:
	rm -f *o usbfw 

rebuild: clean all

.PHONY: all clean rebuild