#ifndef COMMON_H
#define COMMON_H

#define _GNU_SOURCE

#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <stdbool.h>
#include <sys/select.h>

#define MAX_PLAYERS 9
#define MIN_BOARD_SIZE 10
#define DEFAULT_WIDTH 10
#define DEFAULT_HEIGHT 10
#define DEFAULT_DELAY 200
#define DEFAULT_TIMEOUT 10
#define MAX_REWARD 9
#define MIN_REWARD 1
#define PLAYER_NAME_SIZE 16

// Direcciones de movimiento (0-7, comenzando arriba y en sentido horario)
#define DIR_UP 0
#define DIR_UP_RIGHT 1
#define DIR_RIGHT 2
#define DIR_DOWN_RIGHT 3
#define DIR_DOWN 4
#define DIR_DOWN_LEFT 5
#define DIR_LEFT 6
#define DIR_UP_LEFT 7

// Nombres de memorias compartidas
#define GAME_STATE_SHM "/game_state"
#define GAME_SYNC_SHM "/game_sync"

// Estructura del jugador
typedef struct
{
    char name[PLAYER_NAME_SIZE]; // Nombre del jugador
    unsigned int score;          // Puntaje
    unsigned int invalid_moves;  // Cantidad de movimientos inválidos
    unsigned int valid_moves;    // Cantidad de movimientos válidos
    unsigned short x, y;         // Coordenadas x e y en el tablero
    pid_t pid;                   // Identificador de proceso
    bool blocked;                // Indica si el jugador está bloqueado
} player_t;

// Estructura del estado del juego
typedef struct
{
    unsigned short width;          // Ancho del tablero
    unsigned short height;         // Alto del tablero
    unsigned int player_count;     // Cantidad de jugadores
    player_t players[MAX_PLAYERS]; // Lista de jugadores
    bool game_finished;            // Indica si el juego se ha terminado
    int board[];                   // Tablero (flexible array member)
} game_state_t;

// Estructura de sincronización
typedef struct
{
    sem_t view_notify;                  // A: El máster le indica a la vista que hay cambios
    sem_t view_done;                    // B: La vista le indica al máster que terminó
    sem_t master_access;                // C: Mutex para evitar inanición del máster
    sem_t state_mutex;                  // D: Mutex para el estado del juego
    sem_t reader_count_mutex;           // E: Mutex para la variable de lectores
    unsigned int reader_count;          // F: Cantidad de jugadores leyendo el estado
    sem_t player_can_move[MAX_PLAYERS]; // G: Indica a cada jugador que puede enviar movimiento
} game_sync_t;

// Funciones auxiliares
void error_exit(const char *msg);
void cleanup_resources(void);
int get_board_cell(game_state_t *state, int x, int y);
void set_board_cell(game_state_t *state, int x, int y, int value);
bool is_valid_position(game_state_t *state, int x, int y);
bool is_cell_free(game_state_t *state, int x, int y);
void get_direction_offset(unsigned char direction, int *dx, int *dy);
void print_usage_master(const char *program_name);
void print_usage_view(const char *program_name);
void print_usage_player(const char *program_name);

#endif
