/* src/shell.c — mini-shell implementation (library portion)
 *
 * Provides the parser and executor.  The `main` entry point
 * lives in src/main.c, kept separate so the test binary can
 * link this file without a `main` collision.
 *
 * Architecture:
 *
 *   shell_parse(line, &cmd)            - line -> AST (shell_cmd)
 *   shell_run(&cmd)                    - AST -> running processes
 *   shell_cmd_print(&cmd, FILE*)       - AST -> pretty-printed text
 */
#include "shell.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>

/* ============================================================
 *                       1. PARSER
 * ============================================================ */

/* Skip leading whitespace, return pointer to first non-ws char. */
static const char *skip_ws(const char *p) {
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

/* Strip a trailing `&` and set `*background` accordingly. */
static void strip_background(char *s, bool *background) {
    *background = false;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    if (n > 0 && s[n - 1] == '&') {
        *background = true;
        s[n - 1] = '\0';
    }
}

/* Tokenize `s` on whitespace, honouring `<` and `>` as separate
   tokens.  Stores pointers into `s` (which is modified in place
   by inserting NULs).  Returns the number of tokens, or -1 on
   overflow.  Sets `*infile` / `*outfile` / `*append` for the
   redirection tokens. */
static int tokenize_stage(char *s, char **argv, int max,
                          char **infile, char **outfile, bool *append) {
    *infile = NULL; *outfile = NULL; *append = false;
    int n = 0;
    char *p = s;
    while (1) {
        p = (char *)skip_ws(p);
        if (*p == '\0') break;
        if (n >= max - 1) return -1;

        /* redirection tokens: `<`, `>`, `>>` */
        if (*p == '<') {
            p = (char *)skip_ws(p + 1);
            char *start = p;
            while (*p && !isspace((unsigned char)*p)) p++;
            if (*p) { *p = '\0'; p++; }
            *infile = start;
            continue;
        }
        if (*p == '>' && p[1] == '>') {
            *append = true;
            p = (char *)skip_ws(p + 2);
            char *start = p;
            while (*p && !isspace((unsigned char)*p)) p++;
            if (*p) { *p = '\0'; p++; }
            *outfile = start;
            continue;
        }
        if (*p == '>') {
            p = (char *)skip_ws(p + 1);
            char *start = p;
            while (*p && !isspace((unsigned char)*p)) p++;
            if (*p) { *p = '\0'; p++; }
            *outfile = start;
            continue;
        }

        /* ordinary word */
        char *start = p;
        while (*p && !isspace((unsigned char)*p)
               && *p != '<' && *p != '>') p++;
        if (*p) { *p = '\0'; p++; }
        argv[n++] = start;
    }
    argv[n] = NULL;
    return n;
}

bool shell_parse(const char *line, shell_cmd *out) {
    memset(out, 0, sizeof *out);
    out->n_stages = 0;
    out->background = false;

    /* Copy the line; we'll mutate it. */
    char buf[SHELL_MAX_LINE];
    size_t len = strlen(line);
    if (len >= sizeof buf) return false;
    memcpy(buf, line, len + 1);

    /* Strip a trailing `&` BEFORE splitting on `|`. */
    strip_background(buf, &out->background);

    /* Empty line (after stripping &) is a parse error. */
    char *p = (char *)skip_ws(buf);
    if (*p == '\0') return false;

    /* Split on `|` into stages.  We don't use strtok_r because
       it silently drops empty trailing fields, which is exactly
       the syntax error we want to detect. */
    char *stage = buf;
    char *bar;
    while (stage) {
        if (out->n_stages >= SHELL_MAX_CMDS) return false;
        bar = strchr(stage, '|');
        if (bar) *bar = '\0';

        /* Trim leading/trailing whitespace from the stage. */
        while (*stage && isspace((unsigned char)*stage)) stage++;
        size_t n = strlen(stage);
        while (n > 0 && isspace((unsigned char)stage[n - 1])) { stage[--n] = '\0'; }

        shell_stage *st = &out->stages[out->n_stages];
        int nt = tokenize_stage(stage, st->argv, SHELL_MAX_TOKENS,
                                &st->infile, &st->outfile, &st->append);
        if (nt < 0) return false;
        if (nt == 0) return false;   /* empty stage */
        out->n_stages++;

        stage = bar ? bar + 1 : NULL;
    }
    if (out->n_stages == 0) return false;
    return true;
}

void shell_cmd_print(const shell_cmd *cmd, FILE *f) {
    for (size_t i = 0; i < cmd->n_stages; i++) {
        const shell_stage *st = &cmd->stages[i];
        fprintf(f, "stage[%zu]:", i);
        for (int j = 0; st->argv[j]; j++) fprintf(f, " [%s]", st->argv[j]);
        if (st->infile)  fprintf(f, "  < %s", st->infile);
        if (st->outfile) fprintf(f, "  %s %s", st->append ? ">>" : ">", st->outfile);
        fputc('\n', f);
    }
    if (cmd->background) fprintf(f, "(background)\n");
}

/* ============================================================
 *                       2. EXECUTOR
 * ============================================================ */

/* Built-in commands.  Return:
     -1  .. not a built-in
     >=0 .. exit status to return without forking
*/
static int try_builtin(char * const *argv) {
    if (argv[0] == NULL) return 0;
    if (strcmp(argv[0], "exit") == 0) {
        int code = argv[1] ? atoi(argv[1]) : 0;
        exit(code);
    }
    if (strcmp(argv[0], "cd") == 0) {
        const char *target = argv[1] ? argv[1] : getenv("HOME");
        if (!target) target = "/";
        if (chdir(target) < 0) { perror("cd"); return 1; }
        return 0;
    }
    return -1;
}

/* Open `path` for redirection in the child. */
static int open_redir(const char *path, int flags, mode_t mode) {
    int fd = open(path, flags, mode);
    if (fd < 0) {
        fprintf(stderr, "shell: open %s: %s\n", path, strerror(errno));
    }
    return fd;
}

/* Run a single stage.  `in_fd` is the fd to use for stdin (or
   -1 to inherit), `out_fd` is the fd for stdout (or -1).  On
   success, never returns.  On error, prints and exits 127. */
static void run_stage(const shell_stage *st, int in_fd, int out_fd) {
    /* Apply redirections. */
    if (st->infile) {
        int fd = open_redir(st->infile, O_RDONLY, 0);
        if (fd < 0) exit(1);
        if (dup2(fd, STDIN_FILENO) < 0) { perror("dup2"); exit(1); }
        close(fd);
    } else if (in_fd >= 0) {
        if (dup2(in_fd, STDIN_FILENO) < 0) { perror("dup2"); exit(1); }
    }
    if (st->outfile) {
        int flags = O_WRONLY | O_CREAT | (st->append ? O_APPEND : O_TRUNC);
        int fd = open_redir(st->outfile, flags, 0644);
        if (fd < 0) exit(1);
        if (dup2(fd, STDOUT_FILENO) < 0) { perror("dup2"); exit(1); }
        close(fd);
    } else if (out_fd >= 0) {
        if (dup2(out_fd, STDOUT_FILENO) < 0) { perror("dup2"); exit(1); }
    }

    /* Try built-in first. */
    int b = try_builtin(st->argv);
    if (b >= 0) exit(b);

    /* External program. */
    execvp(st->argv[0], st->argv);
    fprintf(stderr, "shell: %s: %s\n", st->argv[0], strerror(errno));
    exit(127);
}

int shell_run(const shell_cmd *cmd) {
    /* General path: N-1 pipes + N children.  Always fork+wait
       so the caller's process is preserved (matters for the
       REPL in src/main.c). */
    int prev_fd = -1;   /* read end of previous pipe, -1 if first */
    pid_t pids[SHELL_MAX_CMDS];
    size_t i;
    for (i = 0; i < cmd->n_stages; i++) {
        int pipe_fd[2] = { -1, -1 };
        if (i + 1 < cmd->n_stages) {
            if (pipe(pipe_fd) < 0) { perror("pipe"); return -1; }
        }
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            if (prev_fd >= 0) close(prev_fd);
            if (pipe_fd[0] >= 0) { close(pipe_fd[0]); close(pipe_fd[1]); }
            return -1;
        }
        if (pid == 0) {
            /* child */
            if (prev_fd >= 0) close(pipe_fd[0]);   /* not used */
            run_stage(&cmd->stages[i], prev_fd, pipe_fd[1]);
        }
        pids[i] = pid;
        if (prev_fd >= 0) close(prev_fd);
        prev_fd = pipe_fd[0];
        if (pipe_fd[1] >= 0) close(pipe_fd[1]);
    }

    /* Parent: if background, don't wait.  If foreground, wait for
       the last stage and return its exit status. */
    if (cmd->background) return 0;

    int status = 0;
    for (i = 0; i < cmd->n_stages; i++) {
        int st;
        waitpid(pids[i], &st, 0);
        if (i + 1 == cmd->n_stages) status = st;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}
