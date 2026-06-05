CC     = gcc
CFLAGS = -Wall -Wextra -O2 -Isrc
SRC    = src/main.c src/game.c
TARGET = mechanico

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGET)

.PHONY: all clean
