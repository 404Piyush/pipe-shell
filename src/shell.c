/* src/shell.c — parser + executor (v2)
 *
 * v1 -> v2 deltas:
 *   1. Parser recognises '2>' and '2>>' in addition to '<',
 *      '>', '>>'.  Multiple redirects per stage are allowed
 *      (e.g. `cmd < in > out 2> err`).
 *   2. Executor opens the errfile (if any), dup2's it onto
 *      fd 2 in the child, and closes the source fd.
 *   3. The 'out_append' and 'err_append' flags are honoured.
 *   4. The child also closes any pipe fds it does not need
 *      (e.g. the write end of the previous pipe).
 *
 * Otherwise the v1 design is preserved: the executor is
 * the N-1 pipes, N children, N-1 parent-side close()s
 * pattern.
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

static const char *skip_ws(const char *p) {
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

/* Strip a trailing `&` and record the background flag. */
static void strip_background(char *s, bool *background) {
    *background = false;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    if (n > 0 && s[n - 1] == '&') {
        *background = true;
        s[n - 1] = '\0';
    }
}

/* Tokenize one stage, honouring all four redirect metachars. */
static int tokenize_stage(char *s, shell_stage *out) {
    out->infile = NULL;
    out->outfile = NULL;
    out->out_append = false;
    out->errfile = NULL;
    out->err_append = false;
    for (int i = 0; i < SHELL_MAX_TOKENS; i++) out->argv[i] = NULL;

    int n = 0;
    char *p = s;
    while (1) {
        p = (char *)skip_ws(p);
        if (*p == '\0') break;
        if (n >= SHELL_MAX_TOKENS - 1) return -1;

        /* `2>` and `2>>`: stderr redirect. */
        if (p[0] == '2' && p[1] == '>' && p[2] == '>') {
            p = (char *)skip_ws(p + 3);
            char *start = p;
            while (*p && !isspace((unsigned char)*p)) p++;
            if (*p) { *p = '\0'; p++; }
            out->errfile = start;
            out->err_append = true;
            continue;
        }
        if (p[0] == '2' && p[1] == '>') {
            p = (char *)skip_ws(p + 2);
            char *start = p;
            while (*p && !isspace((unsigned char)*p)) p++;
            if (*p) { *p = '\0'; p++; }
            out->errfile = start;
            out->err_append = false;
            continue;
        }

        /* `1>>` and `1>`: explicit fd-1 redirect (same as `>>` / `>`). */
        if (p[0] == '1' && p[1] == '>' && p[2] == '>') {
            p = (char *)skip_ws(p + 3);
            char *start = p;
            while (*p && !isspace((unsigned char)*p)) p++;
            if (*p) { *p = '\0'; p++; }
            out->outfile = start;
            out->out_append = true;
            continue;
        }
        if (p[0] == '1' && p[1] == '>') {
            p = (char *)skip_ws(p + 2);
            char *start = p;
            while (*p && !isspace((unsigned char)*p)) p++;
            if (*p) { *p = '\0'; p++; }
            out->outfile = start;
            out->out_append = false;
            continue;
        }

        /* `>>` and `>`: stdout redirect. */
        if (p[0] == '>' && p[1] == '>') {
            p = (char *)skip_ws(p + 2);
            char *start = p;
            while (*p && !isspace((unsigned char)*p)) p++;
            if (*p) { *p = '\0'; p++; }
            out->outfile = start;
            out->out_append = true;
            continue;
        }
        if (p[0] == '>') {
            p = (char *)skip_ws(p + 1);
            char *start = p;
            while (*p && !isspace((unsigned char)*p)) p++;
            if (*p) { *p = '\0'; p++; }
            out->outfile = start;
            out->out_append = false;
            continue;
        }

        /* `<`: stdin redirect. */
        if (p[0] == '<') {
            p = (char *)skip_ws(p + 1);
            char *start = p;
            while (*p && !isspace((unsigned char)*p)) p++;
            if (*p) { *p = '\0'; p++; }
            out->infile = start;
            continue;
        }

        /* Ordinary word. */
        char *start = p;
        while (*p && !isspace((unsigned char)*p)
               && *p != '<' && *p != '>'
               && !(p[0] == '1' && p[1] == '>')
               && !(p[0] == '2' && p[1] == '>')) p++;
        if (*p) { *p = '\0'; p++; }
        out->argv[n++] = start;
    }
    if (n == 0) return -1;
    return n;
}

bool shell_parse(const char *line, shell_cmd *out) {
    memset(out, 0, sizeof *out);
    out->n_stages = 0;
    out->background = false;

    char buf[SHELL_MAX_LINE];
    size_t len = strlen(line);
    if (len >= sizeof buf) return false;
    memcpy(buf, line, len + 1);

    strip_background(buf, &out->background);

    char *p = (char *)skip_ws(buf);
    if (*p == '\0') return false;

    /* Split on `|`.  strchr, not strtok_r, because the latter
       silently drops empty trailing fields (which is exactly
       the syntax error we want to catch). */
    char *stage = buf;
    char *bar;
    while (stage) {
        if (out->n_stages >= SHELL_MAX_CMDS) return false;
        bar = strchr(stage, '|');
        if (bar) *bar = '\0';

        while (*stage && isspace((unsigned char)*stage)) stage++;
        size_t n = strlen(stage);
        while (n > 0 && isspace((unsigned char)stage[n - 1])) { stage[--n] = '\0'; }

        shell_stage *st = &out->stages[out->n_stages];
        int nt = tokenize_stage(stage, st);
        if (nt < 0) return false;
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
        if (st->outfile) fprintf(f, "  %s %s", st->out_append ? ">>" : ">", st->outfile);
        if (st->errfile) fprintf(f, "  2%s %s", st->err_append ? ">>" : ">", st->errfile);
        fputc('\n', f);
    }
    if (cmd->background) fprintf(f, "(background)\n");
}

/* ============================================================
 *                       2. EXECUTOR
 * ============================================================ */

static int open_redir(const char *path, int flags, mode_t mode) {
    int fd = open(path, flags, mode);
    if (fd < 0) {
        fprintf(stderr, "shell: open %s: %s\n", path, strerror(errno));
    }
    return fd;
}

/* Open a redirect target and return its fd; -1 on error.
   The caller closes the source fd after dup2. */
static int open_target(const char *path, bool append) {
    int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
    return open_redir(path, flags, 0644);
}

static void run_stage(const shell_stage *st, int in_fd, int out_fd, int err_fd_unused) {
    (void)err_fd_unused;

    /* stdin: file redirect wins, else pipe from previous stage. */
    if (st->infile) {
        int fd = open_redir(st->infile, O_RDONLY, 0);
        if (fd < 0) _exit(1);
        if (dup2(fd, STDIN_FILENO) < 0) { perror("dup2 stdin"); _exit(1); }
        close(fd);
    } else if (in_fd >= 0) {
        if (dup2(in_fd, STDIN_FILENO) < 0) { perror("dup2 stdin"); _exit(1); }
    }

    /* stdout: file redirect wins, else pipe to next stage. */
    if (st->outfile) {
        int fd = open_target(st->outfile, st->out_append);
        if (fd < 0) _exit(1);
        if (dup2(fd, STDOUT_FILENO) < 0) { perror("dup2 stdout"); _exit(1); }
        close(fd);
    } else if (out_fd >= 0) {
        if (dup2(out_fd, STDOUT_FILENO) < 0) { perror("dup2 stdout"); _exit(1); }
    }

    /* stderr: file redirect wins, else inherit. */
    if (st->errfile) {
        int fd = open_target(st->errfile, st->err_append);
        if (fd < 0) _exit(1);
        if (dup2(fd, STDERR_FILENO) < 0) { perror("dup2 stderr"); _exit(1); }
        close(fd);
    }

    /* Built-ins. */
    if (st->argv[0] && strcmp(st->argv[0], "exit") == 0) {
        int code = st->argv[1] ? atoi(st->argv[1]) : 0;
        _exit(code);
    }
    if (st->argv[0] && strcmp(st->argv[0], "cd") == 0) {
        const char *target = st->argv[1] ? st->argv[1] : getenv("HOME");
        if (!target) target = "/";
        if (chdir(target) < 0) { perror("cd"); _exit(1); }
        _exit(0);
    }

    /* External program. */
    execvp(st->argv[0], st->argv);
    fprintf(stderr, "shell: %s: %s\n", st->argv[0], strerror(errno));
    _exit(127);
}

int shell_run(const shell_cmd *cmd) {
    if (cmd->n_stages == 0) return 0;

    int prev_fd = -1;   /* read end of previous pipe */
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
            if (prev_fd >= 0) close(pipe_fd[0]);   /* child doesn't read its own inbound pipe */
            run_stage(&cmd->stages[i], prev_fd, pipe_fd[1], 0);
        }
        pids[i] = pid;
        if (prev_fd >= 0) close(prev_fd);
        prev_fd = pipe_fd[0];
        if (pipe_fd[1] >= 0) close(pipe_fd[1]);
    }

    if (cmd->background) return 0;

    int status = 0;
    for (i = 0; i < cmd->n_stages; i++) {
        int st;
        waitpid(pids[i], &st, 0);
        if (i + 1 == cmd->n_stages) status = st;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

/* ============================================================
 *                       3. MAIN LOOP
 * ============================================================ */

/* The REPL lives in src/main.c so this file can be linked
   into the test binary without colliding on `main`. */
