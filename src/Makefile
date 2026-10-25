CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -Iimg $(shell pkg-config --cflags sdl2 SDL2_image SDL2_ttf)
LDLIBS  = $(shell pkg-config --libs sdl2 SDL2_image SDL2_ttf) -lm

TARGET  = app
SRC     = ui/ui.c img/img.c
OBJ     = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDLIBS)

%.o: %.c img/img.h
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJ) $(TARGET)

clean-img:
	rm -r grid/ words/

.PHONY: all run clean
