#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

static int send_move(unsigned char dir) {
    if (dir > 7) dir = 255;                 // 255 = inválido (si te sirve)
    ssize_t n = write(STDOUT_FILENO, &dir, 1);
    if (n != 1) {
        fprintf(stderr, "PLAYER: write fallo (n=%zd, errno=%d)\n", n, errno);
        return -1;
    }
    fprintf(stderr, "PLAYER: mande dir=%u\n", (unsigned)dir);
    return 0;
}

int main(int argc, char *argv[]) 
{
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <width> <height>\n", argv[0]);
        return 1;
    }
    
    int width = atoi(argv[1]);
    int height = atoi(argv[2]);

    // Nunca imprimas en stdout: tu stdout es el pipe hacia el master.
    fprintf(stderr, "PLAYER: init %dx%d, pid=%d\n", width, height, getpid());

    // Evitar que el proceso muera con SIGPIPE si el master cierra el pipe.
    signal(SIGPIPE, SIG_IGN);

    // Para probar: mandamos 10 movimientos al azar (0..7)
    srand((unsigned)time(NULL) ^ (unsigned)getpid());
    for (int i = 0; i < 10; i++) {
        unsigned char dir = (unsigned char)(rand() % 8); // 0..7
        if (send_move(dir) != 0) break;
        usleep(200 * 1000); // 200 ms para no spamear
    }

    fprintf(stderr, "PLAYER: fin\n");
    return 0;
}
