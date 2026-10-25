
CC     = gcc
CFLAGS = -Wall -Wextra -O2

TARGET = solver
SRC    = solver.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(TARGET)

.PHONY: all clean
