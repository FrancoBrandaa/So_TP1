// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "common.h"

// Variables globales para limpieza
static int state_shm_fd = -1;
static int sync_shm_fd = -1;
static game_state_t *game_state = NULL;
static game_sync_t *game_sync = NULL;
static pid_t *player_pids = NULL;
static pid_t view_pid = -1;
static int **player_pipes = NULL;
static int player_count = 0;

// Configuración del juego
typedef struct
{
    int width;
    int height;
    int delay;
    int timeout;
    unsigned int seed;
    char *view_path;
    char **player_paths;
    int player_count;
} game_config_t;

void cleanup_resources(void)
{
    // Cerrar pipes
    if (player_pipes) // los vuelvo a cerrar aca porque en caso de salir del gameloop de forma anticipada me aseguro de cerrarlos
    {
        for (int i = 0; i < player_count; i++)
        {
            if (player_pipes[i])
            {
                if (player_pipes[i][0] != -1)
                    close(player_pipes[i][0]);
                if (player_pipes[i][1] != -1)
                    close(player_pipes[i][1]);
                free(player_pipes[i]);
            }
        }
        free(player_pipes);
    }

    if (game_state) // saco el mapeo de memoria en mi proceso
        munmap(game_state, sizeof(game_state_t) + sizeof(int) * game_state->width * game_state->height);
    if (game_sync)
        munmap(game_sync, sizeof(game_sync_t));
    if (state_shm_fd != -1) // libera los descriptores
        close(state_shm_fd);
    if (sync_shm_fd != -1)
        close(sync_shm_fd);

    shm_unlink(GAME_STATE_SHM); // elimina la entrada a la shared memory
    shm_unlink(GAME_SYNC_SHM);  // hasta que los procesos que la usan no la cierren
                                // el kernel no liberara la memoria
    if (player_pids)            // esto se hace en el master porque se supne que es el ultimo bro
        free(player_pids);
}

void signal_handler(int sig)
{
    (void)sig;
    cleanup_resources();
    exit(EXIT_FAILURE);
}

void parse_arguments(int argc, char *argv[], game_config_t *config)
{
    // Inicializar configuración por defecto
    config->width = DEFAULT_WIDTH;
    config->height = DEFAULT_HEIGHT;
    config->delay = DEFAULT_DELAY;
    config->timeout = DEFAULT_TIMEOUT;
    config->seed = time(NULL);
    config->view_path = NULL;
    config->player_paths = NULL;
    config->player_count = 0;

    int opt;
    bool players_found = false;

    while ((opt = getopt(argc, argv, "w:h:d:t:s:v:p:")) != -1)
    {
        switch (opt)
        {
        case 'w':
            config->width = atoi(optarg);
            if (config->width < MIN_BOARD_SIZE)
            {
                fprintf(stderr, "Width must be at least %d\n", MIN_BOARD_SIZE);
                exit(EXIT_FAILURE);
            }
            break;
        case 'h':
            config->height = atoi(optarg);
            if (config->height < MIN_BOARD_SIZE)
            {
                fprintf(stderr, "Height must be at least %d\n", MIN_BOARD_SIZE);
                exit(EXIT_FAILURE);
            }
            break;
        case 'd':
            config->delay = atoi(optarg);
            break;
        case 't':
            config->timeout = atoi(optarg);
            break;
        case 's':
            config->seed = (unsigned int)atoi(optarg);
            break;
        case 'v':
            config->view_path = optarg;
            break;
        case 'p':
            players_found = true;
            // Contar jugadores restantes
            config->player_count = argc - optind + 1;
            if (config->player_count > MAX_PLAYERS)
            {
                fprintf(stderr, "Maximum %d players allowed\n", MAX_PLAYERS);
                exit(EXIT_FAILURE);
            }
            // Reserva y verificación (V522: posible NULL dereference)
            config->player_paths = malloc(config->player_count * sizeof(char *));
            if (!config->player_paths)
            {
                perror("malloc player_paths");
                exit(EXIT_FAILURE);
            }
            config->player_paths[0] = optarg;

            for (int i = 1; i < config->player_count && optind < argc; i++)
            {
                config->player_paths[i] = argv[optind++];
            }
            break;
        default:
            print_usage_master(argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (!players_found || config->player_count == 0)
    {
        fprintf(stderr, "At least one player is required\n");
        print_usage_master(argv[0]);
        exit(EXIT_FAILURE);
    }
}

void initialize_shared_memory(game_config_t *config)
{
    size_t state_size = sizeof(game_state_t) + sizeof(int) * config->width * config->height;

    // Crear memoria compartida para el estado
    state_shm_fd = shm_open(GAME_STATE_SHM, O_CREAT | O_RDWR, 0666);
    if (state_shm_fd == -1)
        error_exit("shm_open state");

    if (ftruncate(state_shm_fd, state_size) == -1)
        error_exit("ftruncate state");

    game_state = mmap(NULL, state_size, PROT_READ | PROT_WRITE, MAP_SHARED, state_shm_fd, 0);
    if (game_state == MAP_FAILED)
        error_exit("mmap state");

    // Crear memoria compartida para sincronización
    sync_shm_fd = shm_open(GAME_SYNC_SHM, O_CREAT | O_RDWR, 0666);
    if (sync_shm_fd == -1)
        error_exit("shm_open sync");

    if (ftruncate(sync_shm_fd, sizeof(game_sync_t)) == -1)
        error_exit("ftruncate sync");

    game_sync = mmap(NULL, sizeof(game_sync_t), PROT_READ | PROT_WRITE, MAP_SHARED, sync_shm_fd, 0);
    if (game_sync == MAP_FAILED)
        error_exit("mmap sync");

    // Inicializar estado del juego
    game_state->width = config->width;
    game_state->height = config->height;
    game_state->player_count = config->player_count;
    game_state->game_finished = false;

    // Inicializar semáforos
    if (sem_init(&game_sync->view_notify, 1, 0) == -1)
        error_exit("sem_init view_notify");
    if (sem_init(&game_sync->view_done, 1, 0) == -1)
        error_exit("sem_init view_done");
    if (sem_init(&game_sync->master_access, 1, 1) == -1)
        error_exit("sem_init master_access");
    if (sem_init(&game_sync->state_mutex, 1, 1) == -1)
        error_exit("sem_init state_mutex");
    if (sem_init(&game_sync->reader_count_mutex, 1, 1) == -1)
        error_exit("sem_init reader_count_mutex");
    game_sync->reader_count = 0;

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (sem_init(&game_sync->player_can_move[i], 1, 1) == -1)
            error_exit("sem_init player_can_move");
    }
}

void initialize_board(game_config_t *config)
{
    srand(config->seed);

    // Llenar tablero con recompensas aleatorias
    for (int y = 0; y < config->height; y++)
    {
        for (int x = 0; x < config->width; x++)
        {
            int reward = MIN_REWARD + rand() % MAX_REWARD;
            set_board_cell(game_state, x, y, reward);
        }
    }
}

void place_players(game_config_t *config)
{
    // Distribución simple: colocar jugadores en esquinas y bordes
    int positions[][2] = {
        {0, 0}, {config->width - 1, 0}, {0, config->height - 1}, {config->width - 1, config->height - 1}, {config->width / 2, 0}, {config->width / 2, config->height - 1}, {0, config->height / 2}, {config->width - 1, config->height / 2}, {config->width / 2, config->height / 2}};

    for (int i = 0; i < config->player_count; i++)
    {
        snprintf(game_state->players[i].name, PLAYER_NAME_SIZE, "Player%d", i + 1);
        game_state->players[i].score = 0;
        game_state->players[i].invalid_moves = 0;
        game_state->players[i].valid_moves = 0;
        game_state->players[i].x = positions[i][0];
        game_state->players[i].y = positions[i][1];
        game_state->players[i].blocked = false;

        // Marcar celda como ocupada
        set_board_cell(game_state, positions[i][0], positions[i][1], -(i + 1));
    }
}

void create_processes(game_config_t *config)
{

    // view
    if (config->view_path)
    {
        view_pid = fork();
        if (view_pid == -1)
            error_exit("fork view");

        if (view_pid == 0)
        {
            char width_str[16], height_str[16];
            snprintf(width_str, sizeof(width_str), "%d", config->width);
            snprintf(height_str, sizeof(height_str), "%d", config->height);

            execl(config->view_path, config->view_path, width_str, height_str, NULL);
            error_exit("execl view");
        }
    }

    player_pids = malloc(config->player_count * sizeof(pid_t)); // almacena el pid de cada player
    if (!player_pids)
        error_exit("malloc player_pids");
    player_pipes = malloc(config->player_count * sizeof(int *)); // almacena los pipes de cada player
    if (!player_pipes)
        error_exit("malloc player_pipes");

    // Crear pipes y procesos de jugadores
    for (int i = 0; i < config->player_count; i++)
    {
        player_pipes[i] = malloc(2 * sizeof(int));
        if (!player_pipes[i])
        {
            error_exit("malloc player_pipes[i]");
        }
        if (pipe(player_pipes[i]) == -1)
        {
            error_exit("pipe");
        }

        player_pids[i] = fork();
        if (player_pids[i] == -1)
            error_exit("fork player");

        if (player_pids[i] == 0)
        {
            // Proceso hijo (jugador)
            close(player_pipes[i][0]);               // Cerrar extremo de lectura
            dup2(player_pipes[i][1], STDOUT_FILENO); // Redirigir stdout al pipe
            close(player_pipes[i][1]);

            char width_str[16], height_str[16];
            snprintf(width_str, sizeof(width_str), "%d", config->width);
            snprintf(height_str, sizeof(height_str), "%d", config->height);

            execl(config->player_paths[i], config->player_paths[i], width_str, height_str, NULL);
            error_exit("execl player");
        }
        else
        {
            // Proceso padre
            close(player_pipes[i][1]); // Cerrar extremo de escritura
            game_state->players[i].pid = player_pids[i];
        }
    }
}

bool process_move(int player_id, unsigned char direction)
{
    if (player_id < 0 || (unsigned int)player_id >= game_state->player_count)
        return false;

    player_t *player = &game_state->players[player_id];
    if (player->blocked)
        return false;

    int dx, dy;
    get_direction_offset(direction, &dx, &dy);

    int new_x = player->x + dx;
    int new_y = player->y + dy;

    // Validar movimiento
    if (!is_cell_free(game_state, new_x, new_y))
    {
        player->invalid_moves++;

        // Después de un movimiento inválido, verificar si el jugador debe ser bloqueado
        if (!player_has_valid_moves(game_state, player_id))
        {
            player->blocked = true;
        }

        return false;
    }

    // Movimiento válido
    int reward = get_board_cell(game_state, new_x, new_y);
    player->score += reward;
    player->valid_moves++;

    // Actualizar posición
    player->x = new_x;
    player->y = new_y;
    set_board_cell(game_state, new_x, new_y, -(player_id + 1));

    // Después de un movimiento válido, verificar si el jugador debe ser bloqueado
    if (!player_has_valid_moves(game_state, player_id))
    {
        player->blocked = true;
    }

    return true;
}

bool player_has_valid_moves(game_state_t *state, unsigned int player_id)
{
    if (player_id >= state->player_count)
        return false;

    player_t *player = &state->players[player_id];

    // Verificar las 8 direcciones
    for (unsigned char dir = 0; dir < 8; dir++)
    {
        int dx, dy;
        get_direction_offset(dir, &dx, &dy);

        int new_x = player->x + dx;
        int new_y = player->y + dy;

        if (is_cell_free(state, new_x, new_y))
        {
            return true; // Encontró al menos un movimiento válido
        }
    }

    return false; // No tiene movimientos válidos
}

bool check_game_end(void)
{
    // Verificar si algún jugador puede moverse
    for (unsigned int i = 0; i < game_state->player_count; i++)
    {
        if (game_state->players[i].blocked)
            continue;

        player_t *player = &game_state->players[i];

        // Verificar las 8 direcciones
        for (unsigned char dir = 0; dir < 8; dir++)
        {
            int dx, dy;
            get_direction_offset(dir, &dx, &dy);

            int new_x = player->x + dx;
            int new_y = player->y + dy;

            if (is_cell_free(game_state, new_x, new_y))
            {
                return false; // Al menos un jugador puede moverse
            }
        }
    }

    return true; // Ningún jugador puede moverse
}

void notify_view(void)
{
    if (view_pid > 0) // porque este chequeo?
    {
        sem_post(&game_sync->view_notify);
        sem_wait(&game_sync->view_done);
    }
}

void game_loop(game_config_t *config)
{
    fd_set readfds;
    struct timeval timeout;
    int max_fd = 0;
    time_t last_valid_move = time(NULL);
    int current_player = 0;

    // Encontrar el descriptor más alto
    for (int i = 0; i < config->player_count; i++)
    {
        if (player_pipes[i][0] > max_fd)
        {
            max_fd = player_pipes[i][0];
        }
    }

    notify_view(); // Mostrar estado inicial

    bool game_finished = false;
    while (!game_finished)
    {
        // Leer estado del juego con protección
        sem_wait(&game_sync->state_mutex);
        game_finished = game_state->game_finished;
        sem_post(&game_sync->state_mutex);
        /*
        chicos el uso el Fd_set complementado con selectes para manejar multiples pipes
        y no quedarme bloqueado esperando a un solo jugador
        1. limpio el set
        2. agrego los pipes de los jugadores activos
        3. llamo a select con timeout de 1 segundo
        4. si select me dice que hay datos leo en round robin
        5. si no hay datos en 1 segundo verifico timeout global
        6. si timeout global se cumplio termino el juego
        */
        FD_ZERO(&readfds);
        bool has_active_players = false;

        // Agregar pipes de jugadores activos al set (con protección)
        sem_wait(&game_sync->state_mutex);
        for (int i = 0; i < config->player_count; i++)
        {
            if (!game_state->players[i].blocked)
            {
                FD_SET(player_pipes[i][0], &readfds);
                has_active_players = true;
            }
        }
        sem_post(&game_sync->state_mutex);

        // Verificar condiciones de fin del juego con protección
        bool should_end = false;
        if (!has_active_players)
        {
            should_end = true;
        }
        else
        {
            // Proteger el acceso para check_game_end()
            sem_wait(&game_sync->state_mutex);
            should_end = check_game_end();
            sem_post(&game_sync->state_mutex);
        }

        if (should_end)
        {
            sem_wait(&game_sync->state_mutex);
            game_state->game_finished = true;
            sem_post(&game_sync->state_mutex);
            break;
        }

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int ready = select(max_fd + 1, &readfds, NULL, NULL, &timeout);

        if (ready == -1)
        {
            // si el select fue interrumpido por una señal no es un error
            //  entonces continua el loop
            if (errno == EINTR)
                continue;
            error_exit("select");
        }

        if (ready == 0)
        {
            // Timeout - verificar timeout global
            if (time(NULL) - last_valid_move > config->timeout)
            {
                // Proteger escritura del flag de fin de juego
                sem_wait(&game_sync->state_mutex);
                game_state->game_finished = true;
                sem_post(&game_sync->state_mutex);
                break;
            }
            continue;
        }

        // Procesar movimientos en round-robin
        bool processed_move = false;
        for (int attempts = 0; attempts < config->player_count && !processed_move; attempts++)
        {
            int player_id = (current_player + attempts) % config->player_count;

            // Verificar si el jugador está bloqueado con protección
            sem_wait(&game_sync->state_mutex);
            bool player_blocked = game_state->players[player_id].blocked;
            sem_post(&game_sync->state_mutex);

            if (player_blocked || !FD_ISSET(player_pipes[player_id][0], &readfds))
            {
                continue;
            }

            unsigned char move;
            ssize_t bytes_read = read(player_pipes[player_id][0], &move, 1);

            if (bytes_read == 0)
            {
                // EOF - jugador bloqueado (con protección)
                sem_wait(&game_sync->state_mutex);
                game_state->players[player_id].blocked = true;
                sem_post(&game_sync->state_mutex);
                close(player_pipes[player_id][0]);
                player_pipes[player_id][0] = -1; // para que no intente cerrarlo de nuevo en cleanup
                continue;
            }

            if (bytes_read == -1)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                { // Si no hay datos disponibles, continuar
                    continue;
                }
                error_exit("read move");
            }

            // Procesar movimiento
            sem_wait(&game_sync->master_access);
            sem_wait(&game_sync->state_mutex);

            bool valid_move = process_move(player_id, move);
            if (valid_move)
            {
                last_valid_move = time(NULL);
            }

            sem_post(&game_sync->state_mutex);
            sem_post(&game_sync->master_access);

            // Verificar si el jugador está bloqueado después del movimiento (con protección)
            sem_wait(&game_sync->state_mutex);
            bool player_still_blocked = game_state->players[player_id].blocked;
            sem_post(&game_sync->state_mutex);

            // Solo notificar al jugador que puede enviar otro movimiento si NO está bloqueado
            if (!player_still_blocked)
            {
                sem_post(&game_sync->player_can_move[player_id]);
            }

            processed_move = true;
            current_player = (player_id + 1) % config->player_count;

            // Notificar a la vista
            notify_view();

            // Esperar delay
            usleep(config->delay * 1000);
        }
    }

    // Notificar fin del juego
    // game_state->game_finished = true;

    // Cerrar pipes de lectura para que los jugadores reciban EOF
    for (int i = 0; i < config->player_count; i++)
    {
        if (player_pipes[i][0] != -1)
        {
            close(player_pipes[i][0]);
            player_pipes[i][0] = -1;
        }
    }

    // Liberar semáforos para que los jugadores salgan de su bucle
    for (int i = 0; i < config->player_count; i++)
    {
        sem_post(&game_sync->player_can_move[i]);
    }

    notify_view();
}

void wait_for_processes(game_config_t *config)
{
    // Esperar jugadores
    for (int i = 0; i < config->player_count; i++)
    {
        int status;
        waitpid(player_pids[i], &status, 0);

        printf("Player %d (PID %d, Score: %u): ", i + 1, player_pids[i], game_state->players[i].score);
        if (WIFEXITED(status))
        {
            printf("exited with code %d\n", WEXITSTATUS(status));
        }
        else if (WIFSIGNALED(status))
        {
            printf("terminated by signal %d\n", WTERMSIG(status));
        }
    }

    // Esperar vista
    if (view_pid > 0)
    {
        int status;
        waitpid(view_pid, &status, 0);
        printf("View (PID %d): ", view_pid);
        if (WIFEXITED(status))
        {
            printf("exited with code %d\n", WEXITSTATUS(status));
        }
        else if (WIFSIGNALED(status))
        {
            printf("terminated by signal %d\n", WTERMSIG(status));
        }
        else
        {
            printf("ended (unknown reason)\n");
        }
    }

    // Liberar array de rutas de players
    if (config->player_paths)
    {
        free(config->player_paths);
        config->player_paths = NULL;
    }
}

int main(int argc, char *argv[])
{
    game_config_t config;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    parse_arguments(argc, argv, &config);
    player_count = config.player_count; // la usan algunas funciones pues es una global pero esstaria bueno que consulten a la config y borrar esta linea

    initialize_shared_memory(&config);
    initialize_board(&config);
    place_players(&config);
    create_processes(&config);

    game_loop(&config);
    wait_for_processes(&config);

    cleanup_resources();
    // verify_resources_after_cleanup();
    return 0;
}
