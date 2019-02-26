CC=avr-gcc
STRIP=strip
UPGDST=/var/www

CFLAGS = -mmcu=atmega8 -DF_CPU=8000000UL -Os  #-S  -gdwarf-2 
HEADERS = #pips.h fakexml.h sockio.h # chatxmpp.h datacode.h

CFLAGS += -Wall --std=gnu99 

OBJ = pikb.elf
LIB = 

world: compile

%.elf: %.c $(HEADERS) Makefile
	$(CC) $(CFLAGS) -o $@ $<

compile: $(OBJ) 
	avr-objcopy -O ihex pikb.elf pikb.hex
	[ -f pikb.hex ] && zip -9v pikb8mhz.zip Makefile pikb.c pikb.elf pikb.hex
	[ -d $(UPGDST) ] && cp pikb.hex $(UPGDST)/
        

clean:
	rm -f *.elf *.hex

