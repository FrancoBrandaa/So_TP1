#include "common.h"

void error_exit(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}
void print_usage_master(const char *program_name) {
    printf("Usage: %s [options] -p player1 [player2 ...]\n", program_name);
    printf("Options:\n");
    printf("  -w width    (default: %d, min: %d)\n", DEFAULT_WIDTH, MIN_BOARD_SIZE);
    printf("  -h height   (default: %d, min: %d)\n", DEFAULT_HEIGHT, MIN_BOARD_SIZE);
    printf("  -d delay    (ms, default: %d)\n", DEFAULT_DELAY);
    printf("  -t timeout  (s, default: %d)\n", DEFAULT_TIMEOUT);
    printf("  -s seed     (default: current time)\n");
    printf("  -v view     (path to view binary, optional)\n");
    printf("  -p players  (paths to player binaries, min: 1, max: %d)\n", MAX_PLAYERS);
}