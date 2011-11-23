CC=gcc
#CFLAGS=-O3 -funroll-loops -c
CFLAGS=-O0 -g -c -g -Wall
#LDFLAGS=-O2 -lm
LDFLAGS=-O2 -lm -g
SOURCES=MyBot.c ants.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=MyBot

all: $(OBJECTS) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f ${EXECUTABLE} ${OBJECTS} *.d

.PHONY: all clean
