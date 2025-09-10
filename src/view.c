#include "common.h"

static game_state_t *game_state = NULL;
static game_sync_t *game_sync = NULL;


//same as cleanup player
void cleanup_view(void) {
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
    cleanup_view();
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

void print_board(void) {
    printf("\n=== ChompChamps Game State ===\n");
    printf("Board Size: %dx%d\n", game_state->width, game_state->height);
    printf("Players: %d\n", game_state->player_count);
    printf("Game Finished: %s\n\n", game_state->game_finished ? "Yes" : "No");
    
    // Imprimir información de jugadores
    printf("Players Status:\n");
    for (unsigned int i = 0; i < game_state->player_count; i++) {
        player_t *p = &game_state->players[i];
        printf("  %s: Position(%d,%d) Score=%u Valid=%u Invalid=%u %s\n",
               p->name, p->x, p->y, p->score, p->valid_moves, p->invalid_moves,
               p->blocked ? "[BLOCKED]" : "");
    }
    printf("\n");
    
    // Imprimir tablero
    printf("Board:\n");
    printf("   ");
    for (int x = 0; x < game_state->width; x++) {
        printf("%2d ", x);
    }
    printf("\n");
    
    for (int y = 0; y < game_state->height; y++) {
        printf("%2d ", y);
        for (int x = 0; x < game_state->width; x++) {
            int cell = get_board_cell(game_state, x, y);
            
            if (cell >= MIN_REWARD && cell <= MAX_REWARD) {
                printf(" %d ", cell); // Recompensa
            } else if (cell <= 0 && cell >= -MAX_PLAYERS) {
                printf("P%d ", -cell); // Jugador
            } else {
                printf(" ? "); // Valor desconocido
            }
        }
        printf("\n");
    }
    printf("\n");
    
    if (game_state->game_finished) {
        printf("=== GAME FINISHED ===\n");
        
        // Encontrar ganador
        int winner = -1;
        unsigned int max_score = 0;
        unsigned int min_valid_moves = UINT32_MAX;
        unsigned int min_invalid_moves = UINT32_MAX;
        
        for (unsigned int i = 0; i < game_state->player_count; i++) {
            player_t *p = &game_state->players[i];
            if (p->score > max_score || 
                (p->score == max_score && p->valid_moves < min_valid_moves) ||
                (p->score == max_score && p->valid_moves == min_valid_moves && p->invalid_moves < min_invalid_moves)) {
                winner = i;
                max_score = p->score;
                min_valid_moves = p->valid_moves;
                min_invalid_moves = p->invalid_moves;
            }
        }
        
        if (winner >= 0) {
            printf("Winner: %s with score %u\n", game_state->players[winner].name, max_score);
        } else {
            printf("Game ended in a tie!\n");
        }
    }
    
    printf("================================\n\n");
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        print_usage_view(argv[0]);
        return EXIT_FAILURE;
    } //esta de mas
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    int width = atoi(argv[1]);
    int height = atoi(argv[2]);
    
    if (width < MIN_BOARD_SIZE || height < MIN_BOARD_SIZE) {
        fprintf(stderr, "Invalid board dimensions\n");
        return EXIT_FAILURE;
    }
    
    connect_shared_memory(width, height);
    
    while (true) {
        // Esperar notificación del máster
        sem_wait(&game_sync->view_notify);
        
        // Imprimir estado
        print_board();
        
        // Notificar al máster que terminamos
        sem_post(&game_sync->view_done);
        
        // Salir si el juego terminó
        if (game_state->game_finished) {
            break;
        }
    }
    
    cleanup_view();
    return 0;
}
