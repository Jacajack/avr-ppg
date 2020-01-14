#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#include "com.h"
#include "midi.h"
#include "synth.h"

//! Midi channel
struct midistatus midi0 = {0};

//! Forces watchdog-based reset
void reset( )
{
	wdt_enable( WDTO_15MS );
	while ( 1 );
}

//! Disable watchdog after reset
void __attribute__( ( naked ) ) __attribute__( ( section( ".init3" ) ) ) watchdogInit( )
{
	MCUSR = 0;
	wdt_disable( );
}

//! LED control function
static inline void ledpwm( uint8_t l1, uint8_t l2, uint8_t l3 )
{
	static uint8_t pwmcnt = 0;

	if ( pwmcnt < l1 ) PORTB |= ( 1 << 0 );
	else PORTB &= ~( 1 << 0 );

	if ( pwmcnt < l2 ) PORTB |= ( 1 << 1 );
	else PORTB &= ~( 1 << 1 );

	if ( pwmcnt < l3 ) PORTB |= ( 1 << 2 );
	else PORTB &= ~( 1 << 2 );

	pwmcnt++;
}

int main( )
{
	// LED init
	DDRB = 7;

	// Init MIDI (UART)
	cominit( 31250 );

	// Init synthesizer state
	synth_init( );

	// Timer 1 generates interrupts with sampling rate frequency
	// fs = F_CPU / 1000
	TCCR1A = 0;
	TCCR1B = ( 1 << CS10 ) | ( 1 << WGM12 );
	OCR1A = 499;
	TCNT1 = 0;
	TIMSK |= ( 1 << OCIE1A );

	// Enable interrupts
	sei( );

	// The main loop (synchronous)
	// The sound is generated inside an interrupt (handled in synth.c)
	while ( 1 )
	{
		// Receive MIDI command and handle reset
		if ( comstatus( ) ) midiproc( &midi0, UDR, 0 );
		if ( midi0.reset ) reset( );
	}

	return 0;
}
