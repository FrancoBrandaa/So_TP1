#include "common.h"

// Programa de prueba simple para verificar las estructuras
int main() {
    printf("Testing ChompChamps structures...\n");
    
    printf("Size of player_t: %zu bytes\n", sizeof(player_t));
    printf("Size of game_state_t (without board): %zu bytes\n", sizeof(game_state_t));
    printf("Size of game_sync_t: %zu bytes\n", sizeof(game_sync_t));
    
    printf("MAX_PLAYERS: %d\n", MAX_PLAYERS);
    printf("MIN_BOARD_SIZE: %d\n", MIN_BOARD_SIZE);
    
    // Test direction offsets
    printf("\nDirection offsets:\n");
    for (int dir = 0; dir < 8; dir++) {
        int dx, dy;
        get_direction_offset(dir, &dx, &dy);
        printf("Direction %d: dx=%d, dy=%d\n", dir, dx, dy);
    }
    
    printf("\nStructures test completed successfully!\n");
    return 0;
}
