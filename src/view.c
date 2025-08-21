#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <width> <height>\n", argv[0]);
        exit(1);
    }
    
    int width = atoi(argv[1]); // a la vista le pasamos ancho 
    int height = atoi(argv[2]); // a la vista le pasamos alto
    
    printf("VIEW: Iniciado con tablero %dx%d\n", width, height);
    
    printf("VIEW: Mostrando tablero inicial:\n");
    for (int i = 0; i < height; i++) {
        printf("VIEW: ");
        for (int j = 0; j < width; j++) {
            printf(". ");  // Celda vacía
        }
        printf("\n");
    }
    
    for (int update = 1; update <= 3; update++) {
        sleep(2);  // Esperar entre actualizaciones
        printf("VIEW: === Actualización %d ===\n", update);
        printf("VIEW: Jugador se movió a posición (%d, %d)\n", update, update);
    }
    
    
    printf("VIEW: Terminando visualización\n");
    return 0;
}
