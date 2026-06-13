/* include/shell.h — public API
 *
 * Mini-shell v2 — extends v1 with stderr redirection and
 * multiple redirects per stage.  Backward-compatible with
 * the v1 AST (the new fields are `errfile` on each stage
 * and the v2-only `redirects` count for diagnostics).
 */
#ifndef SHELL_H
#define SHELL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/* ---------- limits ---------- */
#define SHELL_MAX_TOKENS  64
#define SHELL_MAX_CMDS    8
#define SHELL_MAX_LINE    4096
#define SHELL_MAX_REDIRS  4   /* <, >, >>, 2> per stage */

/* ---------- a single stage in a pipeline ---------- */
typedef struct {
    char  *argv[SHELL_MAX_TOKENS];   /* NULL-terminated, argv[0]=program */
    char  *infile;                   /* "< FILE"  or NULL */
    char  *outfile;                  /* "> FILE" or NULL */
    bool   out_append;               /* true for ">>"  */
    char  *errfile;                  /* "2> FILE" or NULL (v2) */
    bool   err_append;               /* true for "2>>" (v2) */
} shell_stage;

/* ---------- a parsed command line ---------- */
typedef struct {
    shell_stage stages[SHELL_MAX_CMDS];
    size_t      n_stages;
    bool        background;
} shell_cmd;

/* ---------- parser ---------- */
bool shell_parse(const char *line, shell_cmd *out);

/* ---------- executor ---------- */
int  shell_run(const shell_cmd *cmd);

/* ---------- pretty-printer ---------- */
void shell_cmd_print(const shell_cmd *cmd, FILE *f);

#endif /* SHELL_H */
