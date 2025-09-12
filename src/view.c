// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "common.h"

static game_state_t *game_state = NULL;
static game_sync_t *game_sync = NULL;

// same as cleanup player
void cleanup_view(void)
{
    cleanup_shared_memory(game_state, game_sync);
}

void signal_handler(int sig)
{
    (void)sig;
    cleanup_view();
    exit(EXIT_FAILURE);
}

void connect_shared_memory_view(int width, int height)
{
    if (connect_shared_memory(width, height, &game_state, &game_sync) != 0)
    {
        error_exit("connect_shared_memory");
    }
}

void print_board(void)
{
    printf("\n=== ChompChamps Game State ===\n");
    printf("Board Size: %dx%d\n", game_state->width, game_state->height);
    printf("Players: %u\n", game_state->player_count);
    printf("Game Finished: %s\n\n", game_state->game_finished ? "Yes" : "No");

    // Imprimir información de jugadores
    printf("Players Status:\n");
    for (unsigned int i = 0; i < game_state->player_count; i++)
    {
        player_t *p = &game_state->players[i];
        printf("  %s: Position(%d,%d) Score=%u Valid=%u Invalid=%u %s\n",
               p->name, p->x, p->y, p->score, p->valid_moves, p->invalid_moves,
               p->blocked ? "[BLOCKED]" : "");
    }
    printf("\n");

    // Imprimir tablero
    printf("Board:\n");
    printf("   ");
    for (int x = 0; x < game_state->width; x++)
    {
        printf("%2d ", x);
    }
    printf("\n");

    for (int y = 0; y < game_state->height; y++)
    {
        printf("%2d ", y);
        for (int x = 0; x < game_state->width; x++)
        {
            int cell = get_board_cell(game_state, x, y);

            if (cell >= MIN_REWARD && cell <= MAX_REWARD)
            {
                printf(" %d ", cell); // Recompensa
            }
            else if (cell <= 0 && cell >= -MAX_PLAYERS)
            {
                printf("P%d ", -cell); // Jugador
            }
            else
            {
                printf(" ? "); // Valor desconocido
            }
        }
        printf("\n");
    }
    printf("\n");

    if (game_state->game_finished)
    {
        printf("=== GAME FINISHED ===\n");

        // Encontrar ganador usando función modularizada
        int winner = find_winner(game_state);

        if (winner >= 0)
        {
            printf("Winner: %s with score %u\n",
                   game_state->players[winner].name,
                   game_state->players[winner].score);
        }
        else
        {
            printf("Game ended in a tie!\n");
        }
    }

    printf("================================\n\n");
    fflush(stdout);
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        print_usage_view(argv[0]);
        return EXIT_FAILURE;
    } // esta de mas

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    int width = atoi(argv[1]);
    int height = atoi(argv[2]);

    if (width < MIN_BOARD_SIZE || height < MIN_BOARD_SIZE)
    {
        fprintf(stderr, "Invalid board dimensions\n");
        return EXIT_FAILURE;
    }

    connect_shared_memory_view(width, height);

    while (true)
    {
        // Esperar notificación del máster
        sem_wait(&game_sync->view_notify);

        // Imprimir estado
        print_board();

        // Notificar al máster que terminamos
        sem_post(&game_sync->view_done);

        // Salir si el juego terminó
        if (game_state->game_finished)
        {
            break;
        }
    }

    cleanup_view();
    return 0;
}
