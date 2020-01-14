#include <avr/interrupt.h>
#include <string.h>
#include "ppg_data.h"
#include "synth.h"

//! Wavetable entry struct
struct wavetable_entry
{
	const uint8_t *ptr_l;
	const uint8_t *ptr_r;
	uint8_t factor;
	uint8_t is_key;
};

//! Currently used wavetable
//! It's volatile, because the main ISR uses it
#define DEFAULT_WAVETABLE_SIZE 61
static volatile struct wavetable_entry current_wavetable[DEFAULT_WAVETABLE_SIZE];

//! Returns a pointer to the wave with certain index (that can later be passed to get_waveform_sample())
static inline const uint8_t *get_waveform_pointer( uint8_t index )
{
	return ppg_waveforms + ( index << 6 );
}

//! Returns a sample from a waveform by index - acts as pgm_read_byte()
static inline uint8_t get_waveform_sample( const uint8_t *ptr, uint8_t sample )
{
	return pgm_read_byte( ptr + sample );
}

//! Reads sample from a 64-byte waveform buffer based on 16-bit phase value
static inline uint8_t get_waveform_sample_by_phase( const uint8_t *ptr, uint16_t phase2b )
{
	// This phase ranges 0-127
	uint8_t phase = ((uint8_t*) &phase2b)[1] >> 1;
	uint8_t half_select = phase & 64;
	phase &= 63; // Poor man's modulo 64

	// Waveform mirroring
	if ( half_select )
		return get_waveform_sample( ptr, phase );
	else
		return 255u - get_waveform_sample( ptr, 63u - phase );
}

//! Reads a single sample based on a wavetable entry
static inline uint8_t get_wavetable_entry_sample( const volatile struct wavetable_entry *e, uint16_t phase2b )
{
	uint8_t sample_l = get_waveform_sample_by_phase( e->ptr_l, phase2b );
	uint8_t sample_r = get_waveform_sample_by_phase( e->ptr_r, phase2b );
	uint8_t factor = e->factor;
	uint16_t mix_l = ( 256 - factor ) * sample_l;
	uint16_t mix_r = factor * sample_r;
	uint16_t mix = mix_l + mix_r;
	return mix >> 8;
}

//! Reads a single sample from the global wavetable
static inline uint8_t get_current_wavetable_sample( uint8_t slot, uint16_t phase2b )
{
	return get_wavetable_entry_sample( current_wavetable + slot, phase2b );
}

//! Custom memset() implementation for volatile memory areas
static inline void memset_volatile( volatile void *dest, uint8_t value, size_t len )
{
	for ( volatile uint8_t *ptr = dest; ptr < (volatile uint8_t*)dest + len; ptr++ )
		*ptr = value;
}


// ---------------------------------------------

// Some DSP type aliases
typedef int8_t audio_signal;
typedef int16_t integrator;
typedef integrator filter1pole;

//! Safe int16_t add (no overflow and underflow)
static inline int16_t safe_add( int16_t a, int16_t b )
{
	if ( a > 0 && b > INT16_MAX - a )
		return INT16_MAX;
	else if ( a < 0 && b < INT16_MIN - a )
		return INT16_MIN;
	return a + b;
}

// A 16-bit overflow/underflow-safe digital integrator
static inline integrator integrator_feed( integrator *i, integrator x )
{
	return *i = safe_add( *i, x );
	// return *i += x;
}

//! A 1 pole filter based on the above integrator
//! \see integrator
static inline audio_signal filter1pole_feed( filter1pole *f, int8_t k, audio_signal x )
{
	integrator_feed( f, ( x - ( *f / 256 ) ) * k );
	return *f / 256;
}


// ---------------------------------------------


/**
	Load a wavetable stored in PPG Wave 2.2 format in PROGMEM into the current wavetable buffer
	\returns a pointer to the next wavetable
*/
const uint8_t *load_wavetable_from_progmem( const uint8_t *data )
{
	// Wipe the current wavetable
	// This requires custom volatile memset
	memset_volatile( current_wavetable, 0, DEFAULT_WAVETABLE_SIZE * sizeof( struct wavetable_entry ) );


	// The fist byte is ignored
	data++;

	// Read wavetable entries up to max wavetable slot number
	uint8_t waveform, pos;
	do
	{
		waveform = pgm_read_byte( data++ );
		pos = pgm_read_byte( data++ );

		current_wavetable[pos].ptr_l = get_waveform_pointer( waveform );
		current_wavetable[pos].ptr_r = NULL;
		current_wavetable[pos].factor = 0;
		current_wavetable[pos].is_key = 1;
	}
	while ( pos < DEFAULT_WAVETABLE_SIZE - 1 );

	// Now, generate interpolation coefficients
	volatile struct wavetable_entry *el = NULL, *er = NULL;
	for ( uint8_t i = 0; i < DEFAULT_WAVETABLE_SIZE; i++ )
	{
		// If the current entry contains a key-wave
		if ( current_wavetable[i].is_key )
		{
			el = &current_wavetable[i];

			// Look for the next key-wave
			for ( uint8_t j = i + 1; j < DEFAULT_WAVETABLE_SIZE; j++ )
			{
				if ( current_wavetable[j].is_key )
				{
					er = &current_wavetable[j];
					break;
				}
			}
		}

		// Total distance between known key-waves and distance from the left one
		uint8_t distance_total = er - el;
		uint8_t distance_l = &current_wavetable[i] - el;

		current_wavetable[i].ptr_l = el->ptr_l;
		current_wavetable[i].ptr_r = er->ptr_l;

		// We have to avoid division by 0 for the last slot
		if ( distance_total != 0 )
			current_wavetable[i].factor = ( 65535 / distance_total * distance_l ) >> 8;
		else
			current_wavetable[i].factor = 0;
	}

	// Return pointer to the next wavetable
	return data;
}

//! Loads n-th requested wavetable from binary format
//! \see load_wavetable()
const uint8_t *load_wavetable_n_from_progmem( const uint8_t *data, uint8_t index )
{
	for ( uint8_t i = 0; i < index + 1; i++ )
		data = load_wavetable_from_progmem( data );
	return data;
}

// ---------------------------------------------

static inline uint16_t adcread( uint8_t mux )
{
	//Read data from selected ADC (VCC as reference volatge)
	ADMUX = ( mux << MUX0 ) | ( 1 << REFS0 ) | ( 1 << ADLAR );
	ADCSRA |= ( 1 << ADSC );
	while ( ADCSRA & ( 1 << ADSC ) );
	return ADC;
}


// ---------------------------------------------


//! The main interrupt - samples are generated here
//! \todo replace with synchronous loop and a spinlock
ISR( TIMER1_COMPA_vect )
{
	// DDS phasor
	static uint16_t dds_phase = 0;
	static uint16_t dds_step = 180;

	// Time counter
	static uint16_t t_ms = 0;
	static uint16_t t_cnt = 0;

	uint8_t adc0 = adcread( 0 ) >> 8;
	uint8_t adc1 = adcread( 1 ) >> 8;

	// The osicllator and the filters
	static filter1pole Fa = 0, Fb = 0;
	audio_signal x = get_current_wavetable_sample( adc0 >> 2, dds_phase ) - 127;
	int8_t k = adc1 >> 1;
	audio_signal y = filter1pole_feed( &Fb, k, filter1pole_feed( &Fa, k, x ) );

	// DAC output
	PORTC = 127 + y;

	// Phase stepping and time update
	dds_phase += dds_step;
	if ( ++t_cnt == SAMPLERATE / 1000 )
	{
		t_cnt = 0;
		t_ms++;
	}
}

//! Synthesizer state init
void synth_init( )
{
	// Resistor ladder outputs
	DDRC = 0xff;

	// ADC
	ADCSRA = ( 1 << ADEN ) | ( 1 << ADPS2 );

	// Load a wavetable
	load_wavetable_n_from_progmem( ppg_wavetable, 18 );
}
