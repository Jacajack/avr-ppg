#	avrsynth - simple AVR synthesizer project
#	Copyright (C) 2019 Jacek Wieczorek <mrjjot@gmail.com>
#	This file is part of liblightmodbus.
#	Avrsynth is free software: you can redistribute it and/or modify
#	it under the terms of the GNU General Public License as published by
#	the Free Software Foundation, either version 3 of the License, or
#	(at your option) any later version.
#	Avrsynth is distributed in the hope that it will be useful,
#	but WITHOUT ANY WARRANTY; without even the implied warranty of
#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#	GNU General Public License for more details.
#	You should have received a copy of the GNU General Public License
#	along with this program.  If not, see <http://www.gnu.org/licenses/>.

F_CPU = 16000000UL
MCU = atmega32

CC = avr-gcc
CFLAGS = -Wall -O3

all: clean force bin/synth.elf
	
bin/synth.elf: src/main.c src/synth.c src/ppg_data.c src/midi.c src/com.c 
	$(CC) $(CFLAGS) -DF_CPU=$(F_CPU) -DNOTE_LIM=$(NOTE_LIM) -mmcu=$(MCU) $^ -o $@
	avr-size -C $@ --mcu=$(MCU)
	
force:
	-mkdir bin

clean:
	-rm -rf bin
	
prog: bin/synth.elf
	avrdude -c usbasp -p m32 -U flash:w:$^
