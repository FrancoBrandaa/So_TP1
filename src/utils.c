// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "common.h"

void error_exit(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

int get_board_cell(game_state_t *state, int x, int y)
{
    if (x < 0 || x >= state->width || y < 0 || y >= state->height)
    {
        return -999; // Valor inválido para indicar fuera de límites
    }
    return state->board[y * state->width + x];
}

void set_board_cell(game_state_t *state, int x, int y, int value)
{
    if (x >= 0 && x < state->width && y >= 0 && y < state->height)
    {
        state->board[y * state->width + x] = value;
    }
}

bool is_valid_position(game_state_t *state, int x, int y)
{
    return x >= 0 && x < state->width && y >= 0 && y < state->height;
}

bool is_cell_free(game_state_t *state, int x, int y)
{
    if (!is_valid_position(state, x, y))
    {
        return false;
    }
    int cell_value = get_board_cell(state, x, y);
    return cell_value >= MIN_REWARD && cell_value <= MAX_REWARD;
}

void get_direction_offset(unsigned char direction, int *dx, int *dy)
{
    switch (direction)
    {
    case DIR_UP:
        *dx = 0;
        *dy = -1;
        break;
    case DIR_UP_RIGHT:
        *dx = 1;
        *dy = -1;
        break;
    case DIR_RIGHT:
        *dx = 1;
        *dy = 0;
        break;
    case DIR_DOWN_RIGHT:
        *dx = 1;
        *dy = 1;
        break;
    case DIR_DOWN:
        *dx = 0;
        *dy = 1;
        break;
    case DIR_DOWN_LEFT:
        *dx = -1;
        *dy = 1;
        break;
    case DIR_LEFT:
        *dx = -1;
        *dy = 0;
        break;
    case DIR_UP_LEFT:
        *dx = -1;
        *dy = -1;
        break;
    default:
        *dx = 0;
        *dy = 0;
        break;
    }
}

void print_usage_master(const char *program_name)
{
    printf("Usage: %s [-w width] [-h height] [-d delay] [-t timeout] [-s seed] [-v view] -p player1 [player2 ...]\n", program_name);
    printf("  -w width   : Board width (default: %d, minimum: %d)\n", DEFAULT_WIDTH, MIN_BOARD_SIZE);
    printf("  -h height  : Board height (default: %d, minimum: %d)\n", DEFAULT_HEIGHT, MIN_BOARD_SIZE);
    printf("  -d delay   : Delay in milliseconds between state updates (default: %d)\n", DEFAULT_DELAY);
    printf("  -t timeout : Timeout in seconds for valid moves (default: %d)\n", DEFAULT_TIMEOUT);
    printf("  -s seed    : Random seed (default: current time)\n");
    printf("  -v view    : Path to view binary (optional)\n");
    printf("  -p players : Paths to player binaries (minimum: 1, maximum: %d)\n", MAX_PLAYERS);
}

void print_usage_view(const char *program_name)
{
    printf("Usage: %s <width> <height>\n", program_name);
}

void print_usage_player(const char *program_name)
{
    printf("Usage: %s <width> <height>\n", program_name);
}

// Funciones genéricas para memoria compartida
void cleanup_shared_memory(game_state_t *game_state, game_sync_t *game_sync)
{
    if (game_state)
    {
        size_t state_size = sizeof(game_state_t) + sizeof(int) * game_state->width * game_state->height;
        munmap(game_state, state_size);
    }
    if (game_sync)
    {
        munmap(game_sync, sizeof(game_sync_t));
    }
}

int connect_shared_memory(int width, int height, game_state_t **game_state, game_sync_t **game_sync)
{
    size_t state_size = sizeof(game_state_t) + sizeof(int) * width * height;

    // Conectar a memoria compartida del estado
    int state_shm_fd = shm_open(GAME_STATE_SHM, O_RDONLY, 0);
    if (state_shm_fd == -1)
        return -1;

    *game_state = mmap(NULL, state_size, PROT_READ, MAP_SHARED, state_shm_fd, 0);
    if (*game_state == MAP_FAILED)
    {
        close(state_shm_fd);
        return -1;
    }
    close(state_shm_fd);

    // Conectar a memoria compartida de sincronización
    int sync_shm_fd = shm_open(GAME_SYNC_SHM, O_RDWR, 0);
    if (sync_shm_fd == -1)
    {
        munmap(*game_state, state_size);
        *game_state = NULL;
        return -1;
    }

    *game_sync = mmap(NULL, sizeof(game_sync_t), PROT_READ | PROT_WRITE, MAP_SHARED, sync_shm_fd, 0);
    if (*game_sync == MAP_FAILED)
    {
        close(sync_shm_fd);
        munmap(*game_state, state_size);
        *game_state = NULL;
        return -1;
    }
    close(sync_shm_fd);

    return 0;
}

// Función para encontrar el ganador del juego
int find_winner(game_state_t *state)
{
    if (!state || state->player_count == 0)
    {
        return -1; // No hay jugadores
    }

    int winner = -1;
    unsigned int max_score = 0;
    unsigned int min_valid_moves = UINT32_MAX;
    unsigned int min_invalid_moves = UINT32_MAX;

    for (unsigned int i = 0; i < state->player_count; i++)
    {
        player_t *p = &state->players[i];

        // Criterios de ganador (en orden de prioridad):
        // 1. Mayor puntaje
        // 2. Si empate en puntaje, menos movimientos válidos (más eficiente)
        // 3. Si empate completo, menos movimientos inválidos
        if (p->score > max_score ||
            (p->score == max_score && p->valid_moves < min_valid_moves) ||
            (p->score == max_score && p->valid_moves == min_valid_moves && p->invalid_moves < min_invalid_moves))
        {

            winner = i;
            max_score = p->score;
            min_valid_moves = p->valid_moves;
            min_invalid_moves = p->invalid_moves;
        }
    }

    return winner;
}
