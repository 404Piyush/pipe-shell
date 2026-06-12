# Showcase

A closer look at pipe-shell.

## The parser

The parser is a three-pass design with no allocation.
It mutates a local copy of the input line in place and
stores pointers into it.

### Pass 1: strip trailing `&`

```c
static void strip_background(char *s, bool *background) {
    *background = false;
    size_t n = strlen(s);
    while (n > 0 && isspace(s[n - 1])) n--;
    if (n > 0 && s[n - 1] == '&') {
        *background = true;
        s[n - 1] = '\0';
    }
}
```

The trailing `&` is removed and recorded in
`cmd->background`. If you don't strip it first, the
tokenizer will treat it as part of the last word.

### Pass 2: split on `|`

We use `strchr`, not `strtok_r`. The reason:
`strtok_r` silently returns NULL when the next field
after a delimiter is empty, which means `ls |` would
parse as a single stage and never produce an error.

The custom loop:

```c
char *stage = buf;
char *bar;
while (stage) {
    bar = strchr(stage, '|');
    if (bar) *bar = '\0';
    /* ...tokenize, increment n_stages, error if empty... */
    stage = bar ? bar + 1 : NULL;
}
```

An empty stage (caused by `||` or `ls |`) returns a parse
error. The C test suite asserts this:

```c
ASSERT_TRUE(!shell_parse("|",   &c));
ASSERT_TRUE(!shell_parse("ls |", &c));
ASSERT_TRUE(!shell_parse("| ls", &c));
```

### Pass 3: tokenize each stage

```c
while (isspace(*p)) p++;
if (*p == '<') { /* infile */ ... }
if (*p == '>' && p[1] == '>') { /* outfile append */ ... }
if (*p == '>') { /* outfile truncate */ ... }
/* ordinary word */
```

`<`, `>`, and `>>` are single-character metacharacters
that consume the next word as the redirection target.

## The executor

30 lines, recursive only in the sense that `shell_run`
calls itself for built-ins via `exit()`.

```c
int prev_fd = -1;
for (i = 0; i < n_stages; i++) {
    int pipe_fd[2] = {-1, -1};
    if (i + 1 < n_stages) pipe(pipe_fd);
    pid_t pid = fork();
    if (pid == 0) {
        run_stage(&stages[i], prev_fd, pipe_fd[1]);
    }
    if (prev_fd >= 0) close(prev_fd);
    prev_fd = pipe_fd[0];
    if (pipe_fd[1] >= 0) close(pipe_fd[1]);
}
if (!background) wait for every child;
```

The crucial detail — and the single most common shell bug
— is `close(prev_fd)` in the parent. If you forget it,
the consumer's `read(2)` blocks forever because the kernel
will not generate EOF until *every* reference to the write
end of the previous pipe is gone. I made this bug twice
while writing the executor.

## Walkthrough: `ls /etc | grep hosts | wc -l`

A three-stage pipeline. The executor does the following
in order:

1. `pipe(fd0)` — the read end will be child 1's stdin.
2. `pipe(fd1)` — the read end will be child 2's stdin.
3. `fork()` — child 0 will run `ls /etc`.
4. `fork()` — child 1 will run `grep hosts`.
5. `fork()` — child 2 will run `wc -l`.

Each child does its `dup2` dance and `execvp`s. The
parent closes `fd0[1]`, `fd1[1]` (write ends, only
relevant in the children), then waits for all three
children in order. The exit status reported is that of
the last stage (`wc -l`).

Total syscalls: 2 `pipe`, 3 `fork`, ~6 `dup2`, ~6 `close`,
3 `execve`, 3 `wait4`. Roughly 23 kernel transitions to
run a 3-stage pipeline.

## Tests

| Parser (8 cases) | Executor (5 cases) |
|------------------|---------------------|
| simple command   | `true` / `false`    |
| input redirection | input redirection |
| output (truncate)| output redirection |
| output (append)  | append redirection |
| pipeline         | pipeline + output |
| pipeline + redir | `cd` built-in      |
| trailing `&`     |                    |
| empty / bad input |                    |

Run `make test`. All 56 assertions must pass.

## Try the live playground

[pipe-shell.404piyush.me](https://pipe-shell.404piyush.me)
has an interactive parser. Type a command line; the AST
renders live as boxes with pipe arrows; the kernel call
sequence renders below. 6 preset examples. No install
required.
