// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "common.h"
#include <ncurses.h>

// Definiciones de colores para jugadores
#define COLOR_PLAYER_1 1
#define COLOR_PLAYER_2 2
#define COLOR_PLAYER_3 3
#define COLOR_PLAYER_4 4
#define COLOR_PLAYER_5 5
#define COLOR_PLAYER_6 6
#define COLOR_PLAYER_7 7
#define COLOR_PLAYER_8 8
#define COLOR_PLAYER_9 9
#define COLOR_REWARDS 10

static game_state_t *game_state = NULL;
static game_sync_t *game_sync = NULL;

void init_colors(void)
{
    if (has_colors())
    {
        start_color();

        // Definir pares de colores para jugadores (texto brillante sobre fondo negro)
        init_pair(COLOR_PLAYER_1, COLOR_RED, COLOR_BLACK);
        init_pair(COLOR_PLAYER_2, COLOR_GREEN, COLOR_BLACK);
        init_pair(COLOR_PLAYER_3, COLOR_BLUE, COLOR_BLACK);
        init_pair(COLOR_PLAYER_4, COLOR_YELLOW, COLOR_BLACK);
        init_pair(COLOR_PLAYER_5, COLOR_MAGENTA, COLOR_BLACK);
        init_pair(COLOR_PLAYER_6, COLOR_CYAN, COLOR_BLACK);
        init_pair(COLOR_PLAYER_7, COLOR_WHITE, COLOR_BLACK);
        init_pair(COLOR_PLAYER_8, COLOR_RED, COLOR_BLACK); // Reutilizamos colores si hay más de 7
        init_pair(COLOR_PLAYER_9, COLOR_GREEN, COLOR_BLACK);

        // Color para recompensas (blanco)
        init_pair(COLOR_REWARDS, COLOR_WHITE, COLOR_BLACK);
    }
}

// same as cleanup player
void cleanup_view(void)
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
    // Limpiar ncurses
    endwin();
}

void signal_handler(int sig)
{
    (void)sig;
    cleanup_view();
    exit(EXIT_FAILURE);
}

void connect_shared_memory(int width, int height)
{
    size_t state_size = sizeof(game_state_t) + sizeof(int) * width * height;

    // Conectar a memoria compartida del estado
    int state_shm_fd = shm_open(GAME_STATE_SHM, O_RDONLY, 0);
    if (state_shm_fd == -1)
        error_exit("shm_open state");

    game_state = mmap(NULL, state_size, PROT_READ, MAP_SHARED, state_shm_fd, 0);
    if (game_state == MAP_FAILED)
        error_exit("mmap state");
    close(state_shm_fd);

    // Conectar a memoria compartida de sincronización
    int sync_shm_fd = shm_open(GAME_SYNC_SHM, O_RDWR, 0);
    if (sync_shm_fd == -1)
        error_exit("shm_open sync");

    game_sync = mmap(NULL, sizeof(game_sync_t), PROT_READ | PROT_WRITE, MAP_SHARED, sync_shm_fd, 0);
    if (game_sync == MAP_FAILED)
        error_exit("mmap sync");
    close(sync_shm_fd);
}

void print_board(void)
{
    clear(); // Limpiar pantalla

    printw("\n=== ChompChamps Game State ===\n");
    printw("Board Size: %dx%d\n", game_state->width, game_state->height);
    printw("Players: %u\n", game_state->player_count);
    printw("Game Finished: %s\n\n", game_state->game_finished ? "Yes" : "No");

    // Imprimir información de jugadores
    printw("=== PLAYERS STATUS ===\n");
    for (unsigned int i = 0; i < game_state->player_count; i++)
    {
        player_t *p = &game_state->players[i];

        // Usar color del jugador para el indicador
        attron(COLOR_PAIR(i + 1) | A_BOLD);
        printw("  [P%u] ", i + 1);
        attroff(COLOR_PAIR(i + 1) | A_BOLD);

        // Nombre y posición
        printw("%s: Pos(%d,%d) Score=%u ", p->name, p->x, p->y, p->score);

        // Barra visual del score (cada asterisco = ~20 puntos)
        int score_bars = p->score / 20;
        if (score_bars > 10)
            score_bars = 10; // Máximo 10 asteriscos

        attron(COLOR_PAIR(i + 1));
        for (int j = 0; j < score_bars; j++)
        {
            printw("*");
        }
        attroff(COLOR_PAIR(i + 1));

        // Stats y estado
        printw(" (Valid:%u Invalid:%u)", p->valid_moves, p->invalid_moves);

        if (p->blocked)
        {
            printw(" [BLOCKED]");
        }

        printw("\n");
    }
    printw("\n");

    // Imprimir tablero
    printw("Board:\n");

    // Números de columnas
    printw("   ");
    for (int x = 0; x < game_state->width; x++)
    {
        printw("%2d ", x);
    }
    printw("\n");

    for (int y = 0; y < game_state->height; y++)
    {
        printw("%2d ", y);
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
                    head_player = i + 1; // +1 porque los jugadores se numeran desde 1
                    break;
                }
            }

            if (is_head)
            {
                // Cabeza del jugador - usar color brillante
                attron(COLOR_PAIR(head_player) | A_BOLD);
                printw("P%d ", head_player);
                attroff(COLOR_PAIR(head_player) | A_BOLD);
            }
            else if (cell >= MIN_REWARD && cell <= MAX_REWARD)
            {
                // Recompensa - color blanco
                attron(COLOR_PAIR(COLOR_REWARDS));
                printw(" %d ", cell);
                attroff(COLOR_PAIR(COLOR_REWARDS));
            }
            else if (cell <= 0 && cell >= -MAX_PLAYERS)
            {
                // Cuerpo del jugador - usar color del jugador pero más tenue
                int player_num = -cell;
                attron(COLOR_PAIR(player_num));
                printw(" # "); // Bloque sólido para el cuerpo
                attroff(COLOR_PAIR(player_num));
            }
            else
            {
                printw(" ? "); // Valor desconocido
            }
        }
        printw("\n");
    }
    printw("\n");

    if (game_state->game_finished)
    {
        printw("=== GAME FINISHED ===\n");

        // Encontrar ganador
        int winner = -1;
        unsigned int max_score = 0;
        unsigned int min_valid_moves = UINT32_MAX;
        unsigned int min_invalid_moves = UINT32_MAX;

        for (unsigned int i = 0; i < game_state->player_count; i++)
        {
            player_t *p = &game_state->players[i];
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

        if (winner >= 0)
        {
            printw("Winner: %s with score %u\n", game_state->players[winner].name, max_score);
        }
        else
        {
            printw("Game ended in a tie!\n");
        }
    }

    printw("================================\n\n");
    refresh(); // Actualizar pantalla
}

void show_final_winner(void)
{
    clear();

    // Banner de juego terminado con mejor formato
    attron(A_BOLD | A_BLINK);
    printw("\n\n");
    printw("  ==========================================\n");
    printw("  |              GAME FINISHED!            |\n");
    printw("  ==========================================\n");
    attroff(A_BOLD | A_BLINK);

    printw("\n");

    // Encontrar ganador
    int winner = -1;
    unsigned int max_score = 0;
    unsigned int min_valid_moves = UINT32_MAX;
    unsigned int min_invalid_moves = UINT32_MAX;

    for (unsigned int i = 0; i < game_state->player_count; i++)
    {
        player_t *p = &game_state->players[i];
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

    if (winner >= 0)
    {
        // Mostrar ganador con mucho estilo
        attron(COLOR_PAIR(winner + 1) | A_BOLD);
        printw("    *** WINNER: %s ***\n", game_state->players[winner].name);
        printw("    Score: %u points\n", max_score);
        printw("    Efficiency: %u valid moves, %u invalid moves\n", min_valid_moves, min_invalid_moves);
        attroff(COLOR_PAIR(winner + 1) | A_BOLD);
    }
    else
    {
        attron(A_BOLD);
        printw("  *** IT'S A TIE! ***\n");
        attroff(A_BOLD);
    }

    printw("\n");
    printw("  ----------- FINAL STANDINGS -----------\n");

    // Mostrar todos los jugadores ordenados por puntaje
    for (unsigned int i = 0; i < game_state->player_count; i++)
    {
        player_t *p = &game_state->players[i];

        attron(COLOR_PAIR(i + 1));
        if (i == (unsigned int)winner)
        {
            attron(A_BOLD);
            printw("  > ");
        }
        else
        {
            printw("    ");
        }

        printw("%-8s: %3u pts", p->name, p->score);

        // Barra visual del score más bonita
        int score_bars = p->score / 10;
        if (score_bars > 20)
            score_bars = 20;
        printw(" [");
        for (int j = 0; j < score_bars; j++)
        {
            printw("#");
        }
        for (int j = score_bars; j < 20; j++)
        {
            printw("-");
        }
        printw("]");

        if (i == (unsigned int)winner)
        {
            attroff(A_BOLD);
        }
        attroff(COLOR_PAIR(i + 1));
        printw("\n");
    }

    printw("  ---------------------------------------\n");
    printw("\n");
    printw("    Thanks for playing ChompChamps!\n");
    printw("    Game will close in 5 seconds...\n\n");

    refresh();

    // Sleep por 5 segundos
    sleep(5);
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

    connect_shared_memory(width, height);

    // Inicializar ncurses
    initscr();
    noecho();    // No mostrar las teclas presionadas
    cbreak();    // Leer teclas inmediatamente
    curs_set(0); // Ocultar cursor
    clear();     // Limpiar pantalla

    // Inicializar colores
    init_colors();

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
            // Mostrar pantalla final con ganador y esperar 5 segundos
            show_final_winner();
            break;
        }
    }

    cleanup_view();
    return 0;
}
