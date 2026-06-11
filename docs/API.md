# API reference — `shell.h`

`mini-shell` exposes three public functions and a couple of
data types for the parsed command tree.

## Data types

### `shell_stage`

A single stage in a pipeline — essentially one command, with
optional redirection.

```c
typedef struct {
    char  *argv[SHELL_MAX_TOKENS];   /* NULL-terminated */
    char  *infile;                   /* "< FILE" if any  */
    char  *outfile;                  /* "> FILE" or ">>" */
    bool  append;                    /* true for >>      */
} shell_stage;
```

`argv[0]` is the program name; `argv[1..]` are the arguments.
The last entry is `NULL` (POSIX `execvp` requirement).

### `shell_cmd`

A full command line.

```c
typedef struct {
    shell_stage stages[SHELL_MAX_CMDS];
    size_t      n_stages;            /* 1 for simple, N for pipeline */
    bool        background;          /* trailing '&'                  */
} shell_cmd;
```

`n_stages` is 1 for `ls -l`, 2 for `ls | wc -l`, etc.

## Functions

### `bool shell_parse(const char *line, shell_cmd *out)`

Splits `line` into the AST.  Returns `true` on success.

Recognised syntax:

| form                  | example                    |
|-----------------------|----------------------------|
| simple command        | `ls -la`                   |
| pipeline              | `ls | grep foo | wc -l`    |
| input redirection     | `wc -l < README.md`        |
| output redirection    | `ls > out.txt`            |
| output append         | `ls >> out.txt`           |
| background            | `sleep 5 &`                |
| built-ins             | `exit [N]`, `cd [DIR]`     |

Returns `false` for empty input, unmatched `|`, etc.

### `int shell_run(const shell_cmd *cmd)`

Forks and execs the parsed command.  Returns the exit status
of the last stage of a foreground pipeline, or `0` for a
background job, or `-1` on internal error (e.g. `fork` failed).

The function does *not* print a prompt; it is the caller's
responsibility to drive the REPL.

### `void shell_cmd_print(const shell_cmd *cmd, FILE *f)`

Pretty-prints the AST to `f`.  Useful for debugging the
parser; called by the `--show` CLI flag.

## Limits

| constant             | value | meaning                            |
|----------------------|-------|------------------------------------|
| `SHELL_MAX_TOKENS`   | 64    | max words per stage                |
| `SHELL_MAX_CMDS`     | 8     | max stages in a pipeline           |
| `SHELL_MAX_LINE`     | 4096  | max input-line length              |
