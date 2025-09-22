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

    int status;

    int child_pid = fork();
    if(child_pid<0)
        error_exit("proxy_player");
    if(child_pid==0) // hijo
        execl("/bin/true", "/bin/true", NULL);

    waitpid(child_pid, &status, 0);
    

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

        // Termino de leer
        sem_wait(&game_sync->reader_count_mutex);
        game_sync->reader_count--;
        if (game_sync->reader_count == 0)// si es el ultimo libero el mutex 
        {
            sem_post(&game_sync->state_mutex);
        }
        sem_post(&game_sync->reader_count_mutex);

        unsigned char move = 0;
        if (!game_finished && !blocked)
        {
            move = status;
        }

        //verifico si se bloqueo en la eleccion del movimiento
        if (game_finished || blocked)
            break;

        // Enviar movimiento al master
        if (write(STDOUT_FILENO, &move, 1) != 1)
            break; // Error o pipe cerrado
    }

    cleanup_player();
    return 0;
}
