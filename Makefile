# project name, resulting binaries will get that name
PROJECT=rs485

# mcu options, clock speed and device
F_CPU=8000000UL
GCCDEVICE=attiny13

# object files going into project
OBJECTS=rs485.o

#avrdude options
FUSES=-U lfuse:w:0x7a:m -U hfuse:w:0xff:m
DEVICE=t13

# additional include directories
INCLUDEDIRS=-I..
# additional libraries to link in
LIBRARIES=

vpath %.cpp ..

#--------------------------------------------------------------
CC=avr-gcc
CXX=avr-g++
OBJCOPY=avr-objcopy
OBJDUMP=avr-objdump
SIZE=avr-size
AVRDUDE=avrdude
REMOVE=rm -f
LD=avr-g++

#generic compiler options
CFLAGS=-I. $(INCLUDEDIRS) -g -mmcu=$(GCCDEVICE) -Os \
	-fpack-struct -fshort-enums             \
	-funsigned-bitfields -funsigned-char -Wall \
	-DBEAMBUS_DATETIME_DISABLED \
	-DBEAMBUS_NORECEIVERCONTROL \
	-DBEAMBUS_NOCRC16

CXXFLAGS=$(CFLAGS) -fno-exceptions -DF_CPU=$(F_CPU)

LDFLAGS=-Wl,-Map,$(PROJECT).map -mmcu=$(GCCDEVICE) $(LIBRARIES)

.PHONY: erase clean

#------------------------------------------------------------

all: clean hex

hex: $(PROJECT).hex $(PROJECT).eep

$(PROJECT).elf: $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $?
	@avr-size $(PROJECT).elf
	@avr-objdump -S $@ > $(PROJECT).lst
		
$(PROJECT).hex: $(PROJECT).elf
	@$(OBJCOPY) -j .text -j .data -O ihex $< $@

$(PROJECT).eep: $(PROJECT).elf
	@-$(OBJCOPY) -j .eeprom --change-section-lma .eeprom=0 -O ihex $< $@

flash: all $(PROJECT).hex $(PROJECT).eep
	$(AVRDUDE) -P usb -B 10 -c usbtiny -p $(DEVICE) $(FUSES) -U flash:w:$(PROJECT).hex -U eeprom:w:$(PROJECT).eep

erase:
	$(AVRDUDE) -P usb -c usbtiny -p $(DEVICE) -e

clean:
	@rm -f $(PROJECT).hex $(PROJECT).eep $(PROJECT).elf *.o *~ *.lst *.map
						 	 		
%.o : %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDEDIRS) -c $< -o $@

%.o : %.c
	$(CC) $(CFLAGS) $(INCLUDEDIRS) -c $< -o $@
