CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -O2
LDLIBS = -lm

.PHONY: all clean test

all: xnor

xnor: xnor.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)
clean:
	$(RM) xnor
