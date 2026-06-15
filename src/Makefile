CC     = gcc
WINCC  = x86_64-w64-mingw32-gcc
CFLAGS = -Wall -Wextra -O2 -Isrc

SRC = src/main.c \
      src/init.c \
      src/party.c \
      src/battle.c \
      src/dungeon.c \
      src/enemy.c \
      src/io.c \
      src/ui.c

# macOS / Linux
all: mechanico

mechanico: $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

# Windows exe (brew install mingw-w64 필요)
windows: $(SRC)
	$(WINCC) $(CFLAGS) -o mechanico.exe $^

clean:
	rm -f mechanico mechanico.exe

.PHONY: all windows clean
