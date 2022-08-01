SRC = firmware.c usb_serial.c
OBJ = firmware.o usb_serial.o
BIN = firmware

.PHONY: all
all: build upload

.PHONY: build
build:
	avr-gcc -g -Os -mmcu=at90usb1286 -c $(SRC)
	avr-gcc -g -mmcu=at90usb1286 -o $(BIN).elf $(OBJ)
	avr-objcopy -j .text -j .data -O ihex $(BIN).elf $(BIN).hex
	rm $(OBJ)
	rm $(BIN).elf

.PHONY: upload
upload:
	sudo dfu-programmer at90usb1286 erase --force
	sudo dfu-programmer at90usb1286 flash $(BIN).hex
	sudo dfu-programmer at90usb1286 reset
