.PHONY: clean

all: packdrop

packdrop: packdrop.c
	$(CC) -W -Wall -g -o $@ $< -lwiringPi -lpthread

clean:
	rm packdrop
