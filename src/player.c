#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <width> <height>\n", argv[0]);
        exit(1);
    }
    
    int width = atoi(argv[1]);
    int height = atoi(argv[2]);
    
    printf("PLAYER: Iniciado en tablero %dx%d, PID=%d\n", width, height, getpid());
    
    char *movements[] = {"ARRIBA", "DERECHA", "ABAJO"};
    int num_movements = 3;
    
    for (int i = 0; i < num_movements; i++) {
        sleep(1);  // Simular "pensamiento" del jugador
        
        // Escribir movimiento por stdout (que está conectado al pipe)
       printf("%s", movements[i]);
       fflush(stdout);  // Asegurar que se envíe inmediatamente
        
        fprintf(stderr, "PLAYER: Envié movimiento '%s'\n", movements[i]);
    }
    
    fprintf(stderr, "PLAYER: Terminando...\n");
    return 0;
}
