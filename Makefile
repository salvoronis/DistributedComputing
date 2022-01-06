CC=clang
CFLAGS=-g -Wall -pedantic -Werror -std=c99

all: runPa4

runPa4:
	rm -f ./pa4/*.log
	$(CC) $(CFLAGS) ./pa4/*.c -o ./pa4/pa4 -L. -lruntime

runPa3:
	rm -f ./pa3/events.log ./pa3/pipes.log
	$(CC) $(CFLAGS) ./pa3/*.c -o ./pa3/pa3 -L. -lruntime

runPa2:
	rm -f ./pa2/events.log ./pa2/pipes.log
	$(CC) $(CFLAGS) ./pa2/*.c -o ./pa2/pa2 -L. -lruntime

runPa1:
	$(CC) $(CFLAGS) ./pa1/*.c -o ./pa1/pa1

clean:
	rm -f ./pa2/pa2