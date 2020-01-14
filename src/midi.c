/*
	avrsynth - simple AVR synthesizer project
	Copyright (C) 2019 Jacek Wieczorek <mrjjot@gmail.com>
	This file is part of liblightmodbus.
	Avrsynth is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.
	Avrsynth is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stddef.h>
#include <inttypes.h>
#include "midi.h"

void midiproc( struct midistatus *midi, uint8_t byte, uint8_t channel )
{
	uint8_t dlim, dcnt, status;
	
	//Null pointer check
	if ( midi == NULL ) return;
	
	//Get configuration from the struct
	dlim = midi->dlim;
	dcnt = midi->dcnt;
	status = midi->status;
	
	//Handle synthesizer reset
	if ( byte == 0xff ) midi->reset = 1;
	
	if ( byte & ( 1 << 7 ) ) //Handle status bytes
	{
		//Extract information from status byte
		status = byte & 0x70;
		dcnt = dlim = 0;
		midi->channel = byte & 0x0f;

		//Check data length for each MIDI command
		switch ( status )
		{
			//Note on
			case 0x10:
				dlim = 2;
				break;
				
			//Note off
			case 0x00:
				dlim = 2;
				break;
			
			//Controller change
			case 0x30:
				dlim = 2;
				break;
			
			//Program change
			case 0x40:
				dlim = 1;
				break;
				
			//Pitch
			case 0x60:
				dlim = 2;
				break;
			
			//Uknown command
			default:
				dlim = 0;
				break;
				
		}
	}
	else if ( midi->channel == channel ) //Handle data bytes
	{
		//Data byte
		midi->dbuf[dcnt++] = byte;
		
		//Interpret command
		if ( dcnt >= dlim )
		{
			switch ( status )
			{
				//Note on
				case 0x10:
					midi->note = midi->dbuf[0];
					midi->notevel = midi->dbuf[1];
					midi->noteon = 1;
					break;
					
				//Note off	
				case 0x00:
					if ( midi->note == midi->dbuf[0] )
						midi->noteon = 0;
					break;
					
				//Controller change
				case 0x30:
					midi->controllers.raw[midi->dbuf[0]] = midi->dbuf[1];
					break;
									
				//Program change
				case 0x40:
					midi->program = midi->dbuf[0];
					break;
					
				//Pitch
				case 0x60:
					midi->pitchbend = midi->dbuf[0] | ( midi->dbuf[1] << 7 );
					break;
					

				default:
					break;
			}
			
			dcnt = 0;
		}
		
	}
	
	//Write config back to the struct
	midi->dlim = dlim;
	midi->dcnt = dcnt;
	midi->status = status;
}
