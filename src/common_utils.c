#include "common.h"

void error_exit(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void print_usage_master(const char *program_name) {
    printf("Usage: %s [options] -p player1 [player2 ...]\n", program_name);
    printf("Options:\n");
    printf("  -w width    (default: %d, min: %d)\n", DEFAULT_WIDTH, MIN_BOARD_SIZE);
    printf("  -h height   (default: %d, min: %d)\n", DEFAULT_HEIGHT, MIN_BOARD_SIZE);
    printf("  -d delay    (ms, default: %d)\n", DEFAULT_DELAY);
    printf("  -t timeout  (s, default: %d)\n", DEFAULT_TIMEOUT);
    printf("  -s seed     (default: current time)\n");
    printf("  -v view     (path to view binary, optional)\n");
    printf("  -p players  (paths to player binaries, min: 1, max: %d)\n", MAX_PLAYERS);
}


int get_board_cell(game_state_t *state, int x, int y) {
    if (x < 0 || x >= state->width || y < 0 || y >= state->height) {
        return -999; // Valor inválido para indicar fuera de límites
    }
    return state->board[y * state->width + x];
}

void set_board_cell(game_state_t *state, int x, int y, int value) {
    if (x >= 0 && x < state->width && y >= 0 && y < state->height) {
        state->board[y * state->width + x] = value;
    }
}



//puede ser interesante esta funcion para hacer mas clean el codigo (a futuro cuando todo corra bien)
/*
void *createSHM(const char *name, size_t size, int *shm_fd) {
    *shm_fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if (*shm_fd == -1) error_exit("shm_open");

    if (ftruncate(*shm_fd, size) == -1) error_exit("ftruncate");

    void *shm_ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, *shm_fd, 0);
    if (shm_ptr == MAP_FAILED) error_exit("mmap");

    return shm_ptr;
}
*/