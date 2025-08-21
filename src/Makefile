CC = gcc
CFLAGS = -Wall -Wextra -std=c99
TARGETS = master view player

all: $(TARGETS)

master: master.c
	$(CC) $(CFLAGS) -o master master.c

view: view.c
	$(CC) $(CFLAGS) -o view view.c

player: player.c
	$(CC) $(CFLAGS) -o player player.c

clean:
	rm -f $(TARGETS)

.PHONY: all clean
