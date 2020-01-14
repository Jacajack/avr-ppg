all:
	clang -o avr_ppg_aplay -Wall avr_ppg_aplay.c -fsanitize=address -g -lm 

run: all
	./avr_ppg_aplay | aplay -r 20000
