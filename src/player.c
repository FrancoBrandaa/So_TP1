// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "common.h"

static game_state_t *game_state = NULL;
static game_sync_t *game_sync = NULL;
static int player_id = -1;
// Para estrategia de un solo jugador
// Estado single-player: recorrido de perímetros (clockwise) dynamic
static int sp_initialized = 0;
static int sp_finished = 0;
static unsigned int sp_turn = 0;
static unsigned char sp_dir = DIR_RIGHT; // dirección cardinal actual

/*
 desmapear (con munmap) las regiones de memoria que el proceso mapeó con mmap
*/
void cleanup_player(void)
{
    cleanup_shared_memory(game_state, game_sync);
    // limpear el pipe del mismo
    // nada dinámico ahora
}

void signal_handler(int sig)
{
    (void)sig;
    cleanup_player();
    exit(EXIT_FAILURE);
}

void connect_shared_memory_player(int width, int height)
{
    if (connect_shared_memory(width, height, &game_state, &game_sync) != 0)
    {
        error_exit("connect_shared_memory");
    }
}

int find_player_id(void)
{
    pid_t my_pid = getpid();

    // Leer estado para encontrar nuestro ID
    sem_wait(&game_sync->reader_count_mutex);
    game_sync->reader_count++;
    if (game_sync->reader_count == 1)
    {
        sem_wait(&game_sync->state_mutex);
    }
    sem_post(&game_sync->reader_count_mutex);

    int id = -1;
    for (unsigned int i = 0; i < game_state->player_count; i++)
    {
        if (game_state->players[i].pid == my_pid)
        {
            id = i;
            break;
        }
    }

    sem_wait(&game_sync->reader_count_mutex);
    game_sync->reader_count--;
    if (game_sync->reader_count == 0)
    {
        sem_post(&game_sync->state_mutex);
    }
    sem_post(&game_sync->reader_count_mutex);

    return id;
}

// utilizo los valores locales guardados para decidir el movimiento
// lo que se hace es buscar en todas las direcciones el mejor reward y me muevo hacia ahi (greedy)
unsigned char choose_move_with_local_data(player_t *my_player, int *local_board, int board_width, int board_height)
{
    unsigned char best_move = 0;
    int best_reward = -1;

    for (unsigned char dir = 0; dir < DIRECTIONS_COUNT; dir++)
    {
        int dx, dy;
        get_direction_offset(dir, &dx, &dy);

        int new_x = my_player->x + dx;
        int new_y = my_player->y + dy;

        // Validar límites usando datos locales
        if (new_x < 0 || new_x >= board_width || new_y < 0 || new_y >= board_height)
            continue;

        // Obtener valor del tablero local
        int cell_value = local_board[new_y * board_width + new_x];
        // Verificar si la celda está libre (valor positivo = recompensa)
        if (cell_value >= MIN_REWARD && cell_value <= MAX_REWARD)
        {
            if (cell_value > best_reward)
            {
                best_reward = cell_value;
                best_move = dir;
            }
        }
    }

    return best_move;
}

static inline int sp_cell_free(int *local_board, int w, int h, int x, int y)
{
    if (x < 0 || x >= w || y < 0 || y >= h)
        return 0;
    int v = local_board[y * w + x];
    return (v >= MIN_REWARD && v <= MAX_REWARD);
}

static int sp_can_move_dir(int *local_board, int w, int h, int x, int y, unsigned char dir)
{
    int dx, dy;
    get_direction_offset(dir, &dx, &dy);
    return sp_cell_free(local_board, w, h, x + dx, y + dy);
}

static unsigned char sp_turn_left(unsigned char d)
{
    switch (d)
    {
    case DIR_UP:
        return DIR_LEFT;
    case DIR_LEFT:
        return DIR_DOWN;
    case DIR_DOWN:
        return DIR_RIGHT;
    default:
        return DIR_UP; // DIR_RIGHT
    }
}
static unsigned char sp_turn_right(unsigned char d)
{
    switch (d)
    {
    case DIR_UP:
        return DIR_RIGHT;
    case DIR_RIGHT:
        return DIR_DOWN;
    case DIR_DOWN:
        return DIR_LEFT;
    default:
        return DIR_UP; // DIR_LEFT
    }
}

static unsigned char choose_move_single_player_perimeter(player_t *my_player, int *local_board, int w, int h)
{
    int x = my_player->x, y = my_player->y;
    if (!sp_initialized)
    {
        sp_initialized = 1;
        sp_dir = DIR_RIGHT;
        sp_turn = 0;
        sp_finished = 0;
        fprintf(stderr, "[WALL] init (%d,%d)\n", x, y);
    }
    if (sp_finished)
        return DIR_RIGHT;

    // mantener mano izquierda en pared: girar izquierda si se puede; luego recto; luego derecha; luego 180.
    unsigned char left = sp_turn_left(sp_dir);
    if (sp_can_move_dir(local_board, w, h, x, y, left))
    {
        sp_dir = left;
        sp_turn++;
        return sp_dir;
    }
    if (sp_can_move_dir(local_board, w, h, x, y, sp_dir))
    {
        sp_turn++;
        return sp_dir;
    }
    unsigned char right = sp_turn_right(sp_dir);
    if (sp_can_move_dir(local_board, w, h, x, y, right))
    {
        sp_dir = right;
        sp_turn++;
        return sp_dir;
    }
    unsigned char back = sp_turn_right(sp_turn_right(sp_dir));
    if (sp_can_move_dir(local_board, w, h, x, y, back))
    {
        sp_dir = back;
        sp_turn++;
        return sp_dir;
    }
    // ninguna libre -> terminado
    sp_finished = 1;
    fprintf(stderr, "[WALL] finished (%d,%d) turns=%u\n", x, y, sp_turn);
    return DIR_RIGHT;
}

int main(int argc, char *argv[])
{
    // Inncesario pues el master les pasa correctamente los parametros
    // Lo dejamos por buena practica
    if (argc != 3)
    {
        print_usage_player(argv[0]);
        return EXIT_FAILURE;
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    int width = atoi(argv[1]);
    int height = atoi(argv[2]);

    if (width < MIN_BOARD_SIZE || height < MIN_BOARD_SIZE)
    {
        fprintf(stderr, "Invalid board dimensions\n");
        return EXIT_FAILURE;
    }
    connect_shared_memory_player(width, height);
    // Encontrar nuestro ID de jugador
    player_id = find_player_id();
    if (player_id == -1)
    {
        fprintf(stderr, "Could not find player ID\n");
        cleanup_player();
        return EXIT_FAILURE;
    }

    while (true)
    {
        // Esperar permiso para moverse
        sem_wait(&game_sync->player_can_move[player_id]);

        sem_wait(&game_sync->reader_count_mutex);
        game_sync->reader_count++;
        if (game_sync->reader_count == 1)
        {
            sem_wait(&game_sync->state_mutex);
        }
        sem_post(&game_sync->reader_count_mutex);

        // Copia todo el estado necesario en variables locales
        bool game_finished = game_state->game_finished;
        bool blocked = game_state->players[player_id].blocked;

        // Copiar datos del jugador actual
        player_t my_player = game_state->players[player_id];

        // Copiar tablero a buffer local (solo lo necesario)
        int local_board[game_state->width * game_state->height];
        memcpy(local_board, game_state->board, sizeof(int) * game_state->width * game_state->height);

        // Copiar dimensiones
        int board_width = game_state->width;
        int board_height = game_state->height;

        //  libero semaforos
        sem_wait(&game_sync->reader_count_mutex);
        game_sync->reader_count--;
        if (game_sync->reader_count == 0)
        {
            sem_post(&game_sync->state_mutex);
        }
        sem_post(&game_sync->reader_count_mutex);

        unsigned char move = 0;
        if (!game_finished && !blocked)
        {
            if (game_state->player_count == 1)
            {
                move = choose_move_single_player_perimeter(&my_player, local_board, board_width, board_height);
                if (sp_finished)
                    break;
            }
            else
                move = choose_move_with_local_data(&my_player, local_board, board_width, board_height);
        }

        if (game_finished || blocked)
            break;

        // Enviar movimiento al master
        if (write(STDOUT_FILENO, &move, 1) != 1)
        {
            break; // Error o pipe cerrado
        }
    }

    cleanup_player();
    return 0;
}
