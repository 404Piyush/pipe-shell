/* src/main.c — entry point for the mini-shell binary
 *
 * Kept in a separate translation unit so that the test binary
 * can link src/shell.c (which provides shell_parse / shell_run)
 * without colliding on the `main` symbol.
 */
#include "shell.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
    /* If invoked with arguments, treat them as one command line. */
    if (argc > 2 && strcmp(argv[1], "--run") == 0) {
        shell_cmd cmd;
        if (!shell_parse(argv[2], &cmd)) {
            fprintf(stderr, "shell: parse error\n");
            return 2;
        }
        if (argc > 3 && strcmp(argv[3], "--show") == 0) {
            shell_cmd_print(&cmd, stderr);
        }
        return shell_run(&cmd);
    }
    if (argc > 1) {
        char line[SHELL_MAX_LINE] = {0};
        for (int i = 1; i < argc; i++) {
            if (i > 1) strncat(line, " ", sizeof line - strlen(line) - 1);
            strncat(line, argv[i], sizeof line - strlen(line) - 1);
        }
        shell_cmd cmd;
        if (!shell_parse(line, &cmd)) {
            fprintf(stderr, "shell: parse error\n");
            return 2;
        }
        return shell_run(&cmd);
    }

    /* Interactive REPL. */
    char line[SHELL_MAX_LINE];
    for (;;) {
        fprintf(stderr, "pipe-shell$ ");
        fflush(stderr);
        if (!fgets(line, sizeof line, stdin)) break;
        size_t n = strlen(line);
        if (n && line[n - 1] == '\n') line[--n] = '\0';
        if (n == 0) continue;
        if (strcmp(line, "exit") == 0) break;
        shell_cmd cmd;
        if (!shell_parse(line, &cmd)) {
            fprintf(stderr, "shell: parse error\n");
            continue;
        }
        shell_run(&cmd);
    }
    fputc('\n', stderr);
    return 0;
}
