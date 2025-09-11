CC = gcc
CFLAGS = -g -Wall -Wextra -std=c99 
TARGETS = view player master

# === Integración Valgrind ===
VALGRIND = valgrind \
	--leak-check=full \
	--show-leak-kinds=all \
	--track-origins=yes \
	--errors-for-leak-kinds=all \
	--exit-on-first-error=no \
	--trace-children=yes \
	--log-file=valgrind-%p.log
	

# Argumentos
MASTER_ARGS ?= -v ./view -p ./player

all: $(TARGETS)

master: master.c utils.c
	$(CC) $(CFLAGS) -o master master.c utils.c

view: view.c utils.c
	$(CC) $(CFLAGS) -o view view.c utils.c 

player: player.c utils.c
	$(CC) $(CFLAGS) -o player player.c utils.c

# Ejecuta master normalmente
run: all
	./master $(MASTER_ARGS)

# Ejecuta master bajo Valgrind (usa MASTER_ARGS). Genera log opcional.
valgrind: all
	$(VALGRIND) ./master $(MASTER_ARGS)
	@echo "Logs por proceso: valgrind-<PID>.log"

clean:
	rm -f $(TARGETS)

.PHONY: all clean run valgrind
