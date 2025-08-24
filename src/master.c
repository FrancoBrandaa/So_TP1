
#include "common_utils.h"
#include "common.h"

#define BUFFER_SIZE 256

// Variables globales para limpieza
static int state_shm_fd = -1;
static int sync_shm_fd = -1;
static game_state_t *game_state = NULL;
static game_sync_t *game_sync = NULL;
static pid_t *player_pids = NULL; //vector con todos los pid
static pid_t view_pid = -1;
static int **player_pipes = NULL; //pipe de cada player un puntero a punteros de int (2 pipes por jugador??)
static int player_count = 0;

// Configuración del juego
typedef struct {
    int width;
    int height;
    int delay;
    int timeout;
    unsigned int seed;
    char *view_path;
    char **player_paths;
    int player_count;
} game_config_t;


void create_processes(game_config_t *config) {
    player_pids = malloc(config->player_count * sizeof(pid_t)); //almacena el pid de cada player
    player_pipes = malloc(config->player_count * sizeof(int*));  //almacena los pipes de cada player

    // Crear pipes y procesos de jugadores
    for (int i = 0; i < config->player_count; i++) {
        player_pipes[i] = malloc(2 * sizeof(int));
        if (pipe(player_pipes[i]) == -1) error_exit("pipe");
        
        player_pids[i] = fork(); //inicializo el proceso player y guardo su pid
        if (player_pids[i] == -1) error_exit("fork player");
        
        if (player_pids[i] == 0) {
            // Proceso hijo (jugador)
            close(player_pipes[i][0]); // Cerrar extremo de lectura
            dup2(player_pipes[i][1], STDOUT_FILENO); // Redirigir stdout al pipe => ENTONCES ESTOY ESCRIBIENDO AL PIPE NO AL STDOUT
            close(player_pipes[i][1]);
            
            // con el objetivo de pasar de entero a string...
            char width_str[16], height_str[16];
            snprintf(width_str, sizeof(width_str), "%d", config->width);
            snprintf(height_str, sizeof(height_str), "%d", config->height);
            
            execl(config->player_paths[i], config->player_paths[i], width_str, height_str, NULL);
            error_exit("execl player");
        } else {
            // Proceso padre
            close(player_pipes[i][1]); // Cerrar extremo de escritura (el master no escribe al jugador solo lo escucha)
            game_state->players[i].pid = player_pids[i];
        }
    }
    
    // Crear proceso de vista si se especificó
    if (config->view_path) {
        view_pid = fork();
        if (view_pid == -1) error_exit("fork view");
        
        if (view_pid == 0) {
            char width_str[16], height_str[16];
            snprintf(width_str, sizeof(width_str), "%d", config->width);
            snprintf(height_str, sizeof(height_str), "%d", config->height);
            
            execl(config->view_path, config->view_path, width_str, height_str, NULL);
            error_exit("execl view");
        }
    }
}

void parse_arguments(int argc, char *argv[], game_config_t *config) {
    // Inicializar configuración por defecto
    // ver de borrar en un rato
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

    while ((opt = getopt(argc, argv, "w:h:d:t:s:v:p:")) != -1) {
        switch (opt) {
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
                if (config->height < MIN_BOARD_SIZE) {
                    fprintf(stderr, "Height must be at least %d\n", MIN_BOARD_SIZE);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'd':
                config->delay = atoi(optarg);
                if (config->delay < 0 || config->delay > 5000) {
                    fprintf(stderr, "Delay must be between 0 and 5000 ms\n");
                    exit(EXIT_FAILURE);
                }
                break;
            case 't':
                config->timeout = atoi(optarg);
                if (config->timeout < 1 || config->timeout > 3600) {
                    fprintf(stderr, "Timeout must be between 1 and 3600 seconds\n");
                    exit(EXIT_FAILURE);
                }
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
                config->player_count = argc - optind + 1; //optind apunta a la posición del siguiente argumento es un int
                if (config->player_count > MAX_PLAYERS) {
                    fprintf(stderr, "Maximum %d players allowed\n", MAX_PLAYERS);
                    exit(EXIT_FAILURE);
                }
                
                config->player_paths = malloc(config->player_count * sizeof(char*));
                // config->player_paths[0] = optarg;
                
                // for (int i = 1; i < config->player_count && optind < argc; i++) {
                //     config->player_paths[i] = argv[optind++];
                // }
                //esto reemplaza las lineas comentedas
                for (int i = 0; i < config->player_count && optind - 1 + i < argc; i++) {
                    config->player_paths[i] = (i == 0) ? optarg : argv[optind++];
                }
                break;
            default:
                print_usage_master(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (!players_found || config->player_count == 0) {
        fprintf(stderr, "At least one player is required\n");
        print_usage_master(argv[0]);
        exit(EXIT_FAILURE);
    }
}

void initialize_shared_memory(game_config_t *config) {
    size_t state_size = sizeof(game_state_t) + sizeof(int) * config->width * config->height;
    
    // memoria compartida para el estado
    // state_shm_fd es la referencia a la memoria compartida (usaremos mmap)
    state_shm_fd = shm_open(GAME_STATE_SHM, O_CREAT | O_RDWR, 0666);
    if (state_shm_fd == -1) error_exit("shm_open state");
    
    //le doy el tamano que necesito a la memoria compartida
    if (ftruncate(state_shm_fd, state_size) == -1) error_exit("ftruncate state");
    
    game_state = mmap(NULL, state_size, PROT_READ | PROT_WRITE, MAP_SHARED, state_shm_fd, 0);
    if (game_state == MAP_FAILED) error_exit("mmap state");
    
    // memoria compartida para sincronización
    sync_shm_fd = shm_open(GAME_SYNC_SHM, O_CREAT | O_RDWR, 0666);
    if (sync_shm_fd == -1) error_exit("shm_open sync");
    
    if (ftruncate(sync_shm_fd, sizeof(game_sync_t)) == -1) error_exit("ftruncate sync");
    
    game_sync = mmap(NULL, sizeof(game_sync_t), PROT_READ | PROT_WRITE, MAP_SHARED, sync_shm_fd, 0);
    if (game_sync == MAP_FAILED) error_exit("mmap sync");

    // Inicializar estado del juego
    game_state->width = config->width;
    game_state->height = config->height;
    game_state->player_count = config->player_count;
    game_state->game_finished = false;
    // Inicializar semáforos
    if (sem_init(&game_sync->view_notify, 1, 0) == -1) error_exit("sem_init view_notify");
    if (sem_init(&game_sync->view_done, 1, 0) == -1) error_exit("sem_init view_done");
    if (sem_init(&game_sync->master_access, 1, 1) == -1) error_exit("sem_init master_access");
    if (sem_init(&game_sync->state_mutex, 1, 1) == -1) error_exit("sem_init state_mutex");
    if (sem_init(&game_sync->reader_count_mutex, 1, 1) == -1) error_exit("sem_init reader_count_mutex");
    game_sync->reader_count = 0;

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (sem_init(&game_sync->player_can_move[i], 1, 1) == -1) error_exit("sem_init player_can_move");
    }
}

int main(int argc, char *argv[]) {
    // Parámetros básicos del tablero
    int width = 10, height = 10;
    
    // Pipes para comunicación con player
    // en la posición 0 se lee y en la 1 se escribe
    int pipe_master_to_player[2];
    int pipe_player_to_master[2];
    
    pid_t player_pid, view_pid;
    
    printf("=== MASTER: Iniciando juego %dx%d ===\n", width, height);
    
    if (pipe(pipe_master_to_player) == -1 || pipe(pipe_player_to_master) == -1) {
        perror("Error creando pipes");
        exit(1);
    }
    
    view_pid = fork(); //fork te pasa el pid del hijo
    if (view_pid == -1) {
        perror("Error creando proceso view");
        exit(1);
    }
    
    if (view_pid == 0) {
        // Ejecutar el proceso view pasándole las dimensiones del tablero
        char width_str[10], height_str[10];
        sprintf(width_str, "%d", width);
        sprintf(height_str, "%d", height);
        
        execl("./view", "view", width_str, height_str, NULL);
        perror("Error ejecutando view");
        exit(1);
    }
    
    player_pid = fork();
    if (player_pid == -1) {
        perror("Error creando proceso player");
        exit(1);
    }
    
    if (player_pid == 0) {
        // Configurar pipes para el player
        // El player escribirá por stdout (fd 1) al master
        dup2(pipe_player_to_master[1], 1);
        
        // Cerrar extremos no usados
        close(pipe_master_to_player[0]);
        close(pipe_master_to_player[1]);
        close(pipe_player_to_master[0]);
        //cierro este channel tambien pues lo reemplaze por la stdout => cuando escribo a stdout => escribo al master
        close(pipe_player_to_master[1]);
        
        // Ejecutar el proceso player
        char width_str[10], height_str[10];
        sprintf(width_str, "%d", width); //para pasar el width y el height a string
        sprintf(height_str, "%d", height);
        
        execl("./player", "player", width_str, height_str, NULL);
        perror("Error ejecutando player");
        exit(1);
    }
    
    close(pipe_master_to_player[0]);
    close(pipe_player_to_master[1]);
    
    printf("MASTER: Procesos creados - View PID=%d, Player PID=%d\n", view_pid, player_pid);
    
    char initial_msg[] = "START_GAME";
    // Enviar mensaje de inicio al player (aunque en este ejemplo el player no lo lee)
    write(pipe_master_to_player[1], initial_msg, strlen(initial_msg) + 1);
    printf("MASTER: Juego iniciado, esperando movimientos...\n");
    
    char buffer[BUFFER_SIZE];
    for (int turn = 0; turn < 3; turn++) {
        if (read(pipe_player_to_master[0], buffer, BUFFER_SIZE) > 0) {
            printf("MASTER: Turno %d - Recibí movimiento: '%s'\n", turn + 1, buffer);
            printf("MASTER: Procesando movimiento...\n");
            sleep(1); // Simular procesamiento
        }
    }
    
    close(pipe_master_to_player[1]);
    close(pipe_player_to_master[0]);
    
    // Esperar que terminen los procesos hijos
    int status;
    waitpid(player_pid, &status, 0);
    printf("MASTER: Player terminó\n");
    
    waitpid(view_pid, &status, 0);
    printf("MASTER: View terminó\n");
    
    printf("=== MASTER: Juego terminado ===\n");
    return 0;
}
