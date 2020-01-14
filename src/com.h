#ifndef COM_H
#define COM_H
#include <avr/io.h>
#include <inttypes.h>

extern void cominit( uint32_t baud );
extern uint8_t comstatus( );
extern uint8_t comrx( );
extern uint8_t comtx( uint8_t b );

#endif

