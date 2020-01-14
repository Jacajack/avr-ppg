# avr-wave
 
This project aims to create an AVR synthesizer based using wavetable synthesis.
 
The hardware layer consists of an Atmega32 (Atmega328 could be used as well) and a resistor ladder.
The code requires more than 16kB of flash memory, because that's how much the waveforms and wavetables need.
Add to that exponential LUTs in the future versions.

I've already implemented wavetable synthesis based on waveforms [extracted](https://jacajack.github.io/music/2019/12/10/PPG-EPROM.html) from PPG's EPROMs.
There are also two simple, chained 1-pole filters, that even sort of work...
It all runs on 32kHz sample rate, which is not bad. On Atmega328, this could be 40kHz if 20MHz oscillator is used.

Certain parts of the code (mainly MIDI interpreter and UART code) are taken from [avrsynth](https://github.com/Jacajack/avrsynth).
Compatibility on the hardware level with that project has been broken, hence this project is not exactly a fork.
I've removed license headers in the files, because updating them is pain. The license, however, stays the same.

Todo list:
 - [x] Wavetable synthesis
 - [x] Simple filters
 - [ ] MIDI controls
 - [ ] Scaling down to Atmega328
 - [ ] a PCB?

The prototype:<br>
![](https://i.imgur.com/ka76hNB.png)


