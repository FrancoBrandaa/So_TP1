
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

#define BUFFER_SIZE 256

int main(int argc, char *argv[]) {
    // Parámetros básicos del tablero
    int width = 10, height = 10;
    
    // Pipes para comunicación con player
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
        close(pipe_player_to_master[1]);
        
        // Ejecutar el proceso player
        char width_str[10], height_str[10];
        sprintf(width_str, "%d", width);
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
