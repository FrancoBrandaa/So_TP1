// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "common.h"

// Códigos ANSI para colores (sin ncurses)
#define ANSI_RESET "\033[0m"
#define ANSI_BOLD "\033[1m"
#define ANSI_BLINK "\033[5m"

// Colores para cada jugador
#define ANSI_PLAYER_1_AND_8 "\033[31m" // Rojo
#define ANSI_PLAYER_2_AND_9 "\033[32m" // Verde
#define ANSI_PLAYER_3 "\033[34m"       // Azul
#define ANSI_PLAYER_4 "\033[33m"       // Amarillo
#define ANSI_PLAYER_5 "\033[35m"       // Magenta
#define ANSI_PLAYER_6 "\033[36m"       // Cyan
#define ANSI_PLAYER_7 "\033[37m"       // Blanco
#define ANSI_REWARDS "\033[37m"        // Blanco para recompensas

static game_state_t *game_state = NULL;
static game_sync_t *game_sync = NULL;

// Función para obtener el código de color ANSI de un jugador
const char *get_player_color(int player_num)
{
    switch (player_num)
    {
    case 0:
    case 7:
        return ANSI_PLAYER_1_AND_8; // Rojo
    case 1:
    case 8:
        return ANSI_PLAYER_2_AND_9; // Verde
    case 2:
        return ANSI_PLAYER_3; // Azul
    case 3:
        return ANSI_PLAYER_4; // Amarillo
    case 4:
        return ANSI_PLAYER_5; // Magenta
    case 5:
        return ANSI_PLAYER_6; // Cyan
    case 6:
        return ANSI_PLAYER_7; // Blanco
    default:
        return ANSI_RESET;
    }
}

// Función para obtener el símbolo del cuerpo de cada jugador
const char *get_player_body_symbol(int player_num)
{
    switch (player_num)
    {
    // Como 7 y 8 usan el mismo color que 0 y 1, usan el distinto símbolo
    case 7:
    case 8:
        return "@"; // @ para jugador 7 y 8
    default:
        return "#"; // # para jugadores 0-6
    }
}

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

    // Imprimir información de jugadores con colores y estilo
    printf("=== PLAYERS STATUS ===\n");
    for (unsigned int i = 0; i < game_state->player_count; i++)
    {
        player_t *p = &game_state->players[i];

        // Usar color del jugador para el indicador con negrita
        printf("%s%s[P%u]%s ", get_player_color(i), ANSI_BOLD, i, ANSI_RESET);

        // Nombre y posición
        printf("%s: Pos(%d,%d) Score=%u ", p->name, p->x, p->y, p->score);

        // Barra visual del score (cada asterisco = ~SCORE_BAR_UNIT_STATE puntos)
        int score_bars = p->score / SCORE_BAR_UNIT_STATE;
        if (score_bars > SCORE_BAR_MAX_STATE)
            score_bars = SCORE_BAR_MAX_STATE; // Máximo SCORE_BAR_MAX_STATE asteriscos

        printf("%s", get_player_color(i));
        for (int j = 0; j < score_bars; j++)
        {
            printf("*");
        }
        printf("%s", ANSI_RESET);

        // Stats y estado
        printf(" (Valid:%u Invalid:%u)", p->valid_moves, p->invalid_moves);

        if (p->blocked)
        {
            printf(" [BLOCKED]");
        }

        printf("\n");
    }
    printf("\n");

    // Imprimir tablero
    printf("Board:\n");

    // Números de columnas
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

            // Verificar si esta posición es la cabeza de algún jugador
            bool is_head = false;
            int head_player = -1;
            for (unsigned int i = 0; i < game_state->player_count; i++)
            {
                if (game_state->players[i].x == x && game_state->players[i].y == y)
                {
                    is_head = true;
                    head_player = i; // Ahora usamos indexación 0-based
                    break;
                }
            }

            if (is_head)
            {
                // Cabeza del jugador - usar color brillante
                printf("%s%sP%d%s ", get_player_color(head_player), ANSI_BOLD, head_player, ANSI_RESET);
            }
            else if (cell >= MIN_REWARD && cell <= MAX_REWARD)
            {
                // Recompensa - color blanco
                printf("%s %d %s", ANSI_REWARDS, cell, ANSI_RESET);
            }
            else if (cell <= 0 && cell >= -MAX_PLAYERS)
            {
                // Cuerpo del jugador - usar color del jugador pero más tenue (sin negrita)
                int player_num = -cell; // Ahora 0-based directo
                printf("%s %s %s", get_player_color(player_num), get_player_body_symbol(player_num), ANSI_RESET);
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

void show_final_winner(void)
{
    printf("\n\n");

    // Banner de juego terminado con mejor formato
    printf("%s%s", ANSI_BOLD, ANSI_BLINK);
    printf("  ==========================================\n");
    printf("  |              GAME FINISHED!            |\n");
    printf("  ==========================================\n");
    printf("%s", ANSI_RESET);

    printf("\n");

    // Encontrar ganador usando función modularizada
    int winner = find_winner(game_state);

    if (winner >= 0)
    {
        // Mostrar ganador con mucho estilo
        printf("%s%s", get_player_color(winner), ANSI_BOLD);
        printf("    *** WINNER: %s ***\n", game_state->players[winner].name);
        printf("    Score: %u points\n", game_state->players[winner].score);
        printf("    Efficiency: %u valid moves, %u invalid moves\n",
               game_state->players[winner].valid_moves,
               game_state->players[winner].invalid_moves);
        printf("%s", ANSI_RESET);
    }
    else
    {
        printf("%s", ANSI_BOLD);
        printf("  *** IT'S A TIE! ***\n");
        printf("%s", ANSI_RESET);
    }

    printf("\n");
    printf("  ----------- FINAL STANDINGS -----------\n");

    // Mostrar todos los jugadores ordenados por puntaje
    for (unsigned int i = 0; i < game_state->player_count; i++)
    {
        player_t *p = &game_state->players[i];

        printf("%s", get_player_color(i));
        if (i == (unsigned int)winner)
        {
            printf("%s", ANSI_BOLD);
            printf("  > ");
        }
        else
        {
            printf("    ");
        }

        printf("%-8s: %3u pts", p->name, p->score);

        // Barra visual del score más bonita
        int score_bars = p->score / SCORE_BAR_UNIT_FINAL;
        if (score_bars > SCORE_BAR_MAX_FINAL)
            score_bars = SCORE_BAR_MAX_FINAL;
        printf(" [");
        for (int j = 0; j < score_bars; j++)
        {
            printf("#");
        }
        for (int j = score_bars; j < SCORE_BAR_MAX_FINAL; j++)
        {
            printf("-");
        }
        printf("]");

        printf("%s", ANSI_RESET);
        printf("\n");
    }

    printf("  ---------------------------------------\n");
    printf("\n");
    printf("    Thanks for playing ChompChamps!\n");

    fflush(stdout);
}

int main(int argc, char *argv[])
{
    // Inncesario pues el master les pasa correctamente los parametros
    // Lo dejamos por buena practica
    if (argc != 3)
    {
        print_usage_view(argv[0]);
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
            // Mostrar pantalla final con ganador
            show_final_winner();
            break;
        }
    }

    cleanup_view();
    return 0;
}
