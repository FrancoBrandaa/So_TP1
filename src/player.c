#include "common.h"

static game_state_t *game_state = NULL;
static game_sync_t *game_sync = NULL;
static int player_id = -1;


/*
 desmapear (con munmap) las regiones de memoria que el proceso mapeó con mmap
*/
void cleanup_player(void) {
    if (game_state) {
        size_t state_size = sizeof(game_state_t) + sizeof(int) * game_state->width * game_state->height;
        munmap(game_state, state_size);
    }
    if (game_sync) {
        munmap(game_sync, sizeof(game_sync_t));
    }
}

void signal_handler(int sig) {
    (void)sig;
    cleanup_player();
    exit(EXIT_FAILURE);
}

void connect_shared_memory(int width, int height) {
    size_t state_size = sizeof(game_state_t) + sizeof(int) * width * height;
    
    // Conectar a memoria compartida del estado
    int state_shm_fd = shm_open(GAME_STATE_SHM, O_RDONLY, 0);
    if (state_shm_fd == -1) error_exit("shm_open state");
    
    game_state = mmap(NULL, state_size, PROT_READ, MAP_SHARED, state_shm_fd, 0);
    if (game_state == MAP_FAILED) error_exit("mmap state");
    close(state_shm_fd);
    
    // Conectar a memoria compartida de sincronización
    int sync_shm_fd = shm_open(GAME_SYNC_SHM, O_RDWR, 0);
    if (sync_shm_fd == -1) error_exit("shm_open sync");
    
    game_sync = mmap(NULL, sizeof(game_sync_t), PROT_READ | PROT_WRITE, MAP_SHARED, sync_shm_fd, 0);
    if (game_sync == MAP_FAILED) error_exit("mmap sync");
    close(sync_shm_fd);
}

int find_player_id(void) {
    pid_t my_pid = getpid();
    
    // Leer estado para encontrar nuestro ID
    sem_wait(&game_sync->reader_count_mutex);
    game_sync->reader_count++;
    if (game_sync->reader_count == 1) {
        sem_wait(&game_sync->state_mutex);
    }
    sem_post(&game_sync->reader_count_mutex);
    
    int id = -1;
    for (unsigned int i = 0; i < game_state->player_count; i++) {
        if (game_state->players[i].pid == my_pid) {
            id = i;
            break;
        }
    }
    
    sem_wait(&game_sync->reader_count_mutex);
    game_sync->reader_count--;
    if (game_sync->reader_count == 0) {
        sem_post(&game_sync->state_mutex);
    }
    sem_post(&game_sync->reader_count_mutex);
    
    return id;
}

unsigned char choose_move(void) {
    // Estrategia simple: buscar la celda adyacente con mayor recompensa
    player_t *me = &game_state->players[player_id];
    
    unsigned char best_move = 0;
    int best_reward = -1;
    
    for (unsigned char dir = 0; dir < 8; dir++) {
        int dx, dy;
        get_direction_offset(dir, &dx, &dy);
        
        int new_x = me->x + dx;
        int new_y = me->y + dy;
        
        if (is_cell_free(game_state, new_x, new_y)) {
            int reward = get_board_cell(game_state, new_x, new_y);
            if (reward > best_reward) {
                best_reward = reward;
                best_move = dir;
            }
        }
    }
    
    return best_move;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        print_usage_player(argv[0]);
        return EXIT_FAILURE;
    }
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    int width = atoi(argv[1]);
    int height = atoi(argv[2]);
    
    if (width < MIN_BOARD_SIZE || height < MIN_BOARD_SIZE) {
        fprintf(stderr, "Invalid board dimensions\n");
        return EXIT_FAILURE;
    }
    
    connect_shared_memory(width, height);
    
    // Encontrar nuestro ID de jugador
    player_id = find_player_id();
    if (player_id == -1) {
        fprintf(stderr, "Could not find player ID\n");
        cleanup_player();
        return EXIT_FAILURE;
    }
    
    while (true) {
        // Esperar permiso para moverse
        sem_wait(&game_sync->player_can_move[player_id]);
        
        // Leer estado del juego
        sem_wait(&game_sync->reader_count_mutex);
        game_sync->reader_count++;
        if (game_sync->reader_count == 1) {
            sem_wait(&game_sync->state_mutex);
        }
        sem_post(&game_sync->reader_count_mutex);
        
        bool game_finished = game_state->game_finished;
        bool blocked = game_state->players[player_id].blocked;
        
        unsigned char move = 0;
        if (!game_finished && !blocked) {
            move = choose_move();
        }
        
        sem_wait(&game_sync->reader_count_mutex);
        game_sync->reader_count--;
        if (game_sync->reader_count == 0) {
            sem_post(&game_sync->state_mutex);
        }
        sem_post(&game_sync->reader_count_mutex);
        
        if (game_finished || blocked) {
            break;
        }
        
        // Enviar movimiento
        if (write(STDOUT_FILENO, &move, 1) != 1) {
            break; // Error o pipe cerrado
        } //acordemosnos que el master redirigio stdout de los players
    }
    
    cleanup_player();
    return 0;
}
