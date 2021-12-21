CC=clang
CFLAGS=-g -Wall -pedantic -Werror -std=c99

all: runPa2

runPa2:
	rm -f ./pa2/events.log ./pa2/pipes.log
	$(CC) $(CFLAGS) ./pa2/*.c -o ./pa2/pa2 -L. -lruntime

runPa1:
	$(CC) $(CFLAGS) ./pa1/*.c -o ./pa1/pa1

clean:
	rm -f ./pa2/pa2