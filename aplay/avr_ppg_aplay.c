#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "evu10_waveforms.h"
#include "evu10_wavetable.h"

/**
	\file avr_ppg_aplay.c
	\author Jacek Wieczorek
	
	\brief A proof-of-concept implementation of wavetable synthesis (based on PPG Wave) meant for AVR devices. 
	
	All calculations are performed using variables no bigger than 16 bits. The file could still probably
	use some optimisations, but I'll leave that for later, when I actually get to work with the real hardware.
	
	For now, this program outputs 8-bit data meant for aplay on stdout. The sampling frequency is configured
	using SAMPLING_FREQ macro.
	
	I've also implemented two 1-pole filters chained together. They work pretty nicely and surely make the sound
	more sophisticated.
	
	There's still a lot of work to do - for example LFO and EGs.
*/

//! I think we can manage that...
#define SAMPLING_FREQ 20000

//! This would be 64, but we don't need the additional 3 waveforms that PPG provides
#define DEFAULT_WAVETABLE_SIZE 61

//! Contains currently used wavetable
static struct wavetable_entry
{
	uint8_t *ptr_l;
	uint8_t *ptr_r;
	uint8_t factor;
	uint8_t is_key;
} current_wavetable[DEFAULT_WAVETABLE_SIZE];

//! Returns a pointer to the wave with certain index (that can later be passed to get_waveform_sample())
static inline uint8_t *get_waveform_pointer( uint8_t index )
{
	return evu10_waveforms + ( index << 6 );
}

//! This shall act as pgm_read_byte()
static inline uint8_t get_waveform_sample( uint8_t *ptr, uint8_t sample )
{
	return ptr[sample];
}

//! Reads sample from a 64-byte waveform buffer based on 16-bit phase value
static inline uint8_t get_waveform_sample_by_phase( uint8_t *ptr, uint16_t phase2b )
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
static inline uint8_t get_wavetable_sample( const struct wavetable_entry *e, uint16_t phase2b )
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
	return get_wavetable_sample( current_wavetable + slot, phase2b );
}

/**
	Load a wavetable stored in PPG Wave 2.2 format into an array of wavetable_entry structs of size wavetable_size
	Returns a pointer to the next wavetable
*/
const uint8_t *load_wavetable( struct wavetable_entry *entries, uint8_t wavetable_size, const uint8_t *data )
{
	// Wipe the wavetable
	memset( entries, 0, wavetable_size * sizeof( struct wavetable_entry ) );

	// The fist byte is ignored
	data++;

	// Read wavetable entries up to wavetable slot 60
	uint8_t waveform, pos;
	do
	{
		waveform = *data++;
		pos = *data++;
		
		entries[pos].ptr_l = get_waveform_pointer( waveform );
		entries[pos].ptr_r = NULL;
		entries[pos].factor = 0;
		entries[pos].is_key = 1;
	}
	while ( pos < wavetable_size - 1 );

	// Now, generate interpolation coefficients
	struct wavetable_entry *el = NULL, *er = NULL;
	for ( uint8_t i = 0; i < wavetable_size; i++ )
	{
		// If the current entry contains a key-wave
		if ( entries[i].is_key )
		{
			el = &entries[i];
			
			// Look for the next key-wave
			for ( uint8_t j = i + 1; j < wavetable_size; j++ )
			{
				if ( entries[j].is_key )
				{
					er = &entries[j];
					break;
				}
			}
		}

		// Total distance between known key-waves and distance from the left one 
		uint8_t distance_total = er - el;
		uint8_t distance_l = &entries[i] - el;

		entries[i].ptr_l = el->ptr_l;
		entries[i].ptr_r = er->ptr_l;

		// We have to avoid division by 0 for the last slot
		if ( distance_total != 0 )
			entries[i].factor = ( 65535 / distance_total * distance_l ) >> 8;
		else
			entries[i].factor = 0;
	}

	// Return pointer to the next wavetable
	return data;
}

//! Loads n-th requested wavetable from binary format
//! \see load_wavetable()
const uint8_t *load_wavetable_n( struct wavetable_entry *entries, uint8_t wavetable_size, const uint8_t *data, uint8_t index )
{
	for ( uint8_t i = 0; i < index + 1; i++ )
		data = load_wavetable( entries, wavetable_size, data );
	return data;
}

//! Safe add (no overflow and underflow)
static inline int16_t safe_add( int16_t a, int16_t b )
{
	if ( a > 0 && b > INT16_MAX - a )
		return INT16_MAX;
	else if ( a < 0 && b < INT16_MIN - a )
		return INT16_MIN;
	return a + b;
}

// A 16-bit overflow/underflow-safe digital integrator
typedef int16_t integrator;
static inline integrator integrator_feed( integrator *i, integrator x )
{
	return *i = safe_add( *i, x );
	// return *i += x;
}

//! A 1 pole filter based on the above integrator
//! \see integrator
typedef int8_t audio_signal;
typedef integrator filter1pole;
static inline audio_signal filter1pole_feed( filter1pole *f, int8_t k, audio_signal x )
{
	integrator_feed( f, ( x - ( *f / 256 ) ) * k );
	return *f / 256;
}

int main( int argc, char **argv )
{
	// Load wavetable
	load_wavetable_n( current_wavetable, DEFAULT_WAVETABLE_SIZE, evu10_wavetable, 18 );

	// The main loop
	while ( 1 )
	{
		// DDS
		static uint16_t phase = 0;
		static float f = 62;
		uint16_t phase_step = 65536 * f / SAMPLING_FREQ;

		// Time counter
		static uint32_t cnt = 0;
		static float t = 0;
		cnt++;
		t = (float)cnt / SAMPLING_FREQ;

		// Waveform generation
		uint8_t sample = get_current_wavetable_sample( 30 + 30 * sin( t ), phase  );
		
		// Two 1-pole filters chained together
		audio_signal x = sample - 127;
		int8_t k = 64 + sin( 32 * t ) * 30;
		static filter1pole Fa = 0, Fb = 0;
		audio_signal y = filter1pole_feed( &Fb, k, filter1pole_feed( &Fa, k, x ) );
		
		// Audio output and phase stepping
		putchar( 127 + y );
		phase += phase_step;
	}

	return 0;
}


// ======================================================   Some previously used code
// Might still be useful later

// A simple 1-pole LP filter
/*
uint8_t x = sample;
static uint16_t integrator_input = 0;
static uint16_t integrator_output = 0;
static uint16_t filter_output = 0;
uint8_t k = 127 + sin( 16 * t ) * 126;
integrator_input = ( ( x - ( filter_output >> 8 ) ) ) * k;
integrator_output += integrator_input;
filter_output = integrator_output;
uint8_t y = filter_output >> 8;
*/

/*
int8_t x = sample - 127;
int8_t k = 80 + sin( 32 * t ) * 79;
static int16_t integrator_input = 0;
static int16_t integrator_output = 0;
static int16_t filter_output = 0;
integrator_input = ( x - filter_output / 256 ) * k;
integrator_output = integrator_output + integrator_input;
filter_output = integrator_output;
uint8_t y = 127 + ( integrator_output / 256 );
*/

// Lowpass filter based on an integrator
/*
static integrator I = 0;
int8_t k = 64 + sin( 32 * t ) * 63;
integrator_update( &I, ( x - I / 256 ) * k );
uint8_t y = 127 + I / 256;
*/

// Resonant filter (broken)
/*
static integrator Ia = 0, Ib = 0;
int8_t k = 33 + sin( 8 * t ) * 30;
int8_t n = 5;
integrator_update( &Ia, ( ( x - Ib / 256 ) * n - Ia / 256 ) * k );
integrator_update( &Ib, Ia );
uint8_t y = 127 + Ib / 256;
*/
