# pipe-shell

A small, dependency-free POSIX-ish command interpreter
in C11.  ~500 lines of C, 56 test assertions, full pipeline
support, built-in `cd` and `exit`.

## Features

- Simple commands:        `ls -la`
- Pipelines:              `ls /usr/include | grep stdio | wc -l`
- Input redirection:      `wc -l < README.md`
- Output redirection:     `ls > out.txt`     (truncate)
                          `ls >> out.txt`    (append)
- Background:             `sleep 5 &`
- Built-in commands:      `exit [N]`, `cd [DIR]`
- Interactive REPL and one-shot CLI (`--run`)

## Limitations (intentional)

- No glob expansion (`*`).
- No variable expansion (`$HOME`).
- No quoting of any kind.
- No `;`, `&&`, `||`.  A line is a single pipeline.
- No job control: background jobs are fire-and-forget.

## Build

```
make
```

Produces a single binary `pipe-shell`.

## Run

Interactive:

```
./pipe-shell
pipe-shell$ echo hello world
hello world
pipe-shell$ ls | head -3
Makefile
README.md
docs
pipe-shell$ exit
```

One-shot:

```
./pipe-shell --run "ls /etc | grep hosts"
./pipe-shell --run "wc -l < Makefile" --show
```

The `--show` flag prints the parsed AST to stderr before
running the command.

## Test

```
make test
```

13 test cases / 56 assertions.  All pass.

## Source layout

```
include/shell.h     public API
src/shell.c         parser + executor (library portion)
src/main.c          CLI entry point
tests/test_shell.c  test cases
tests/test_util.h   tiny zero-dependency test framework
docs/API.md         API reference
docs/ARCHITECTURE.md how the pieces fit
```

## License

MIT.  See `LICENSE`.
