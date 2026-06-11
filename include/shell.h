/* include/shell.h — mini-shell public API
 *
 * The shell is small enough to be a single translation unit
 * (`src/shell.c`) but we expose a header so tests can call the
 * parser and executor directly.
 */
#ifndef SHELL_H
#define SHELL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/* --- limits --- */
#define SHELL_MAX_TOKENS  64
#define SHELL_MAX_CMDS    8     /* max stages in a pipeline */
#define SHELL_MAX_LINE    4096

/* --- a single stage in a pipeline --- */
typedef struct {
    char  *argv[SHELL_MAX_TOKENS];   /* NULL-terminated, with
                                        argv[0] = program name */
    char  *infile;                   /* stdin redirection, or NULL */
    char  *outfile;                  /* stdout redirection, or NULL */
    bool  append;                    /* true for >>, false for > */
} shell_stage;

/* --- a parsed command line --- */
typedef struct {
    shell_stage stages[SHELL_MAX_CMDS];
    size_t      n_stages;            /* 1 for simple command, >1 for pipeline */
    bool        background;          /* trailing '&' */
} shell_cmd;

/* --- parser ---
 *
 *   shell_parse(line, out)
 *       Splits `line` on `|` first, then on whitespace within each
 *       stage.  Honours `<`, `>`, `>>` redirection and trailing `&`.
 *       Returns true on success, false on syntax error.
 */
bool shell_parse(const char *line, shell_cmd *out);

/* --- executor ---
 *
 *   shell_run(&cmd)
 *       Forks and execs the parsed command.  Handles a pipeline
 *       of up to SHELL_MAX_CMDS stages with `<`/`>` redirection.
 *       Returns the exit status of the LAST stage in a pipeline,
 *       or -1 on internal error.
 */
int  shell_run(const shell_cmd *cmd);

/* --- pretty-printer (for debug + tests) --- */
void shell_cmd_print(const shell_cmd *cmd, FILE *f);

#endif /* SHELL_H */
