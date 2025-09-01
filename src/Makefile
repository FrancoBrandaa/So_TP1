CC = gcc
CFLAGS = -Wall -Wextra -std=c99
TARGETS =  view player

all: $(TARGETS)

view: view.c utils.c
	$(CC) $(CFLAGS) -o view view.c utils.c

player: player.c utils.c
	$(CC) $(CFLAGS) -o player player.c utils.c

clean:
	rm -f $(TARGETS)

.PHONY: all clean
