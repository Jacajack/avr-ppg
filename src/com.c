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

#include <avr/io.h>
#include <inttypes.h>
#include "com.h"

//Initialize USART
void cominit( uint32_t baud )
{
	baud = F_CPU / 16 / baud - 1;
	
	UBRRH = (unsigned char) ( baud >> 8 ); //Set baud rate
    UBRRL = (unsigned char) baud;
    
    UCSRB = ( 1 << RXEN ) | ( 1 << TXEN ); //Enable RX and TX
    UCSRC = ( 1 << URSEL ) | ( 0 << USBS ) | ( 3 << UCSZ0 ); //Set data format - 8 bit data, 1 stop
}

uint8_t comstatus( )
{
	return UCSRA & ( 1 << RXC );
}

//Receive character
uint8_t comrx( )
{
	while ( !( UCSRA & ( 1 << RXC ) ) );
	return UDR;
}

//Transmit character
uint8_t comtx( uint8_t b )
{
	while ( !( UCSRA & ( 1 << UDRE ) ) );
	UDR = b;
	return b;
}
