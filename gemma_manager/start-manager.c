#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

/*
 * start-manager: compiled wrapper to launch the Gemma TUI start-manager
 *
 * Usage: ./start-manager
 */
int main(int argc, char **argv) {
    /* Ensure TERM is set for curses */
    if (!getenv("TERM")) {
        setenv("TERM", "xterm-256color", 1);
    }
    /* Execute the Python TUI entry point */
    execlp("python3", "python3", "cli.py", "tui", (char *)NULL);
    perror("Failed to launch TUI");
    return EXIT_FAILURE;
}
