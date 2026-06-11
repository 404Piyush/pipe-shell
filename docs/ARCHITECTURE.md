# Architecture — `mini-shell`

A short tour of how the three parts of the program
interact, with line-level pointers into `src/shell.c`.

## High-level data flow

```
$ ls /etc | grep hosts > out.txt
                   |
                   v
        +-------------------+
        |   shell_parse()   |   (line)  -> (shell_cmd)
        +-------------------+
                   |
                   v
        +-------------------+
        |   shell_run()     |   (shell_cmd) -> running procs
        +-------------------+
                   |
        +----------+----------+
        v          v          v
      fork       fork       fork
   (child:    (child:     (child:
    ls /etc)   grep hosts) cat > out.txt)
```

The parent does no I/O of its own; it just wires fds and
reaps children.

## The parser

`shell_parse()` does three things, in order:

1. **Strip background flag** — `strip_background()` removes
   a trailing `&` and records it in `cmd->background`.

2. **Split on `|`** — we walk the buffer with `strchr('|')`
   rather than `strtok_r()` because `strtok_r()` silently
   drops empty trailing fields, which is exactly the
   syntax error we want to detect.

3. **Tokenize each stage** — `tokenize_stage()` splits
   on whitespace, but treats `<`, `>`, and `>>` as
   single-character metacharacters that consume the next
   word as the redirection target.

The parser never allocates; it stores pointers into a
local copy of the input line.

## The executor

`shell_run()` implements the general pipeline recipe from
week 10:

```c
int prev_fd = -1;
for (i = 0; i < n_stages; i++) {
    int pipe_fd[2] = {-1, -1};
    if (i + 1 < n_stages) pipe(pipe_fd);
    pid_t pid = fork();
    if (pid == 0) {
        /* child: dup2 from prev_fd, dup2 to pipe_fd[1], exec */
        run_stage(&stages[i], prev_fd, pipe_fd[1]);
    }
    /* parent: close the read end of the previous pipe,
       remember the read end of THIS pipe as prev_fd */
    if (prev_fd >= 0) close(prev_fd);
    prev_fd = pipe_fd[0];
    if (pipe_fd[1] >= 0) close(pipe_fd[1]);
}
```

The crucial cleanup is closing `prev_fd` after the fork:
if you forget it, the reader's `read(2)` will block
forever because the kernel won't generate EOF until
**every** reference to the write end is gone.

## Built-ins

`try_builtin()` is called from `run_stage()` in the child
*after* redirection.  It handles `exit` and `cd`; the
child then `exit()`s with the appropriate status.  This
means built-ins happen in a forked child rather than in
the shell itself, which is harmless for `exit` and is
*necessary* for `cd` (because `chdir()` would otherwise
change the parent's CWD).

The REPL sees the same fork+wait sequence it would for
an external command, which is why the loop doesn't blow
up on `cd` or `exit`.

## References

- `man 2 fork`, `man 2 pipe`, `man 2 dup2`, `man 3 exec`
- L.  Rosemblum and J. Ousterhout, *The Design and
  Implementation of a Log-Structured File System*
- B.  Kernighan and R. Pike, *The Unix Programming
  Environment*, Ch. 1 (the original `hoc` shell)
