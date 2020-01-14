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
