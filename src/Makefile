CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -fsanitize=address -fsanitize=leak -fno-omit-frame-pointer
TARGETS = master view player

all: $(TARGETS)

master: master.c
	$(CC) $(CFLAGS) -o master master.c common_utils.c

view: view.c
	$(CC) $(CFLAGS) -o view view.c common_utils.c

player: player.c
	$(CC) $(CFLAGS) -o player player.c common_utils.c

clean:
	rm -f $(TARGETS)

.PHONY: all clean
