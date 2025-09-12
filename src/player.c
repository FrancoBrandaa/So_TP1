// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "common.h"

static game_state_t *game_state = NULL;
static game_sync_t *game_sync = NULL;
static int player_id = -1;

/*
 desmapear (con munmap) las regiones de memoria que el proceso mapeó con mmap
*/
void cleanup_player(void)
{
    cleanup_shared_memory(game_state, game_sync);
    // limpear el pipe del mismo
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
//  lo que se hace es buscar en todas las direcciones el mejor reward y me muevo segun el mejor (greedy)
unsigned char choose_move_with_local_data(player_t *my_player, int *local_board, int board_width, int board_height)
{
    unsigned char best_move = 0;
    int best_reward = -1;

    for (unsigned char dir = 0; dir < 8; dir++)
    {
        int dx, dy;
        get_direction_offset(dir, &dx, &dy);

        int new_x = my_player->x + dx;
        int new_y = my_player->y + dy;

        // Validar límites usando datos locales
        if (new_x < 0 || new_x >= board_width || new_y < 0 || new_y >= board_height)
        {
            continue;
        }

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

        // tipico problema de lectores escritores xd
        sem_wait(&game_sync->reader_count_mutex);
        game_sync->reader_count++;
        if (game_sync->reader_count == 1)
        {
            sem_wait(&game_sync->state_mutex);
        }
        sem_post(&game_sync->reader_count_mutex);

        // COPIAR TODO EL ESTADO NECESARIO A VARIABLES LOCALES
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
            // Usar datos locales para calcular movimiento
            move = choose_move_with_local_data(&my_player, local_board, board_width, board_height);

            // SLEEP SOLO PARA EL PLAYER 0 (para testing)
            if (player_id == 0)
            {
                //sleep(11); // Player 0 es lento
            }
        }

        if (game_finished || blocked)
        {
            break;
        }

        // Enviar movimiento al master
        if (write(STDOUT_FILENO, &move, 1) != 1)
        {
            break; // Error o pipe cerrado
        }
    }

    cleanup_player();
    return 0;
}
