# pipe-shell

A small, dependency-free **POSIX-ish command interpreter**
in C11. ~500 lines of C, full pipeline support, redirection,
backgrounding, built-ins, and a testable library API.

> **[pipe-shell.404piyush.me](https://pipe-shell.404piyush.me)** —
> a live editorial page with an interactive playground. Type a
> command line; the AST renders live; the kernel call
> sequence animates step by step.

```
include/shell.h         public API  (~30 lines)
src/shell.c             parser + executor  (~250 lines)
src/main.c              CLI entry point (REPL or --run)
tests/test_shell.c      test suite  (13 cases, 56 assertions)
docs/API.md             API reference
docs/ARCHITECTURE.md    how the pieces fit
```

## Why

A shell is the universal Swiss-army knife of Unix. A
500-line shell that handles pipelines correctly is the
smallest version of `bash` worth understanding. Once you
can read it, the rest of Unix — daemons, init, job
control, container runtimes — falls into place.

This is that 500 lines. It parses a command line, splits
it on `|`, applies redirections, forks a child for each
stage, and reaps the foreground pipeline in order. The
executor is about 30 lines of C.

## Quick start

```sh
git clone https://github.com/404Piyush/pipe-shell
cd pipe-shell
make test        # 56 assertions, 13 cases
./pipe-shell     # interactive REPL
./pipe-shell --run "ls | grep hosts | wc -l"  # one-shot
```

## Public API

```c
typedef struct {
    char *argv[SHELL_MAX_TOKENS];   /* NULL-terminated */
    char *infile;                   /* "< FILE" if any   */
    char *outfile;                  /* "> FILE" or ">>"  */
    bool  append;
} shell_stage;

typedef struct {
    shell_stage stages[SHELL_MAX_CMDS];
    size_t      n_stages;
    bool        background;
} shell_cmd;

bool shell_parse    (const char *line, shell_cmd *out);
int  shell_run      (const shell_cmd *cmd);
void shell_cmd_print(const shell_cmd *cmd, FILE *f);
```

The full reference is in [`docs/API.md`](docs/API.md).

## What's supported

| Form                  | Example                          |
|-----------------------|----------------------------------|
| simple command        | `ls -la`                         |
| pipeline              | `ls \| grep foo \| wc -l`        |
| input redirection     | `wc -l < file.txt`               |
| output (truncate)     | `ls > out.txt`                   |
| output (append)       | `ls >> out.txt`                  |
| background            | `sleep 5 &`                      |
| built-ins             | `exit [N]`, `cd [DIR]`           |

## What's deliberately missing

- No glob expansion (`*`)
- No variable expansion (`$HOME`)
- No quoting of any kind
- No `;`, `&&`, `||`
- No job control (background jobs are fire-and-forget)

Each missing feature is a 100 to 200 line change on top of
the existing structure. They are good follow-up work, not
part of the minimum.

## Worked example

```sh
$ ./pipe-shell
pipe-shell$ ls /etc | grep hosts | wc -l
       3
pipe-shell$ echo hello world > /tmp/out.txt
pipe-shell$ cat /tmp/out.txt
hello world
pipe-shell$ sleep 0.1 & done
pipe-shell$ exit
```

## The pipeline recipe

The executor is 30 lines. The shape is:

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

The four rules of pipeline programming:

1. The parent closes its copy of every pipe fd. If it does
   not, the consumer's `read(2)` blocks forever because the
   kernel will not generate EOF until *every* reference to
   the write end is gone.
2. The child `dup2`s the read end of its input pipe to
   stdin, and the write end of its output pipe to stdout.
3. The child closes every fd it does not need before
   `exec`.
4. The parent `waitpid`s every child for a foreground
   pipeline, or none of them for a background pipeline.

## Tests

56 assertions across 13 test cases. The suite covers:

**Parser (8 cases):**
- Simple commands
- Input redirection
- Output redirection (truncate and append)
- Multi-stage pipelines
- Pipeline with redirection in the last stage
- Trailing `&` (background)
- Empty / malformed input

**Executor (5 cases):**
- `true` / `false` exit codes
- Input redirection
- Output redirection
- Append redirection
- Pipeline with output redirection
- The `cd` built-in

Run `make test`.

## Limitations

- **No glob or variable expansion.** This is a parser, not
  a fully-fledged shell. Add it on top if you need it.
- **No quoting.** Whitespace is a separator, period.
- **No `;`, `&&`, `||`.** A line is a single pipeline.
- **No job control.** Background jobs cannot be `fg`'d or
  `bg`'d. They are fire-and-forget.
- **Single-threaded.** The shell itself is single-threaded;
  the children it forks are the operating system's problem.

## Project layout

```
pipe-shell/
├── README.md
├── CHANGELOG.md
├── LICENSE
├── Makefile
├── include/shell.h
├── src/shell.c
├── src/main.c
├── tests/test_shell.c
├── tests/test_counters.c
├── tests/test_util.h
├── docs/
│   ├── API.md
│   └── ARCHITECTURE.md
├── site/                static landing page (Vercel)
│   ├── index.html
│   ├── shell.js
│   ├── app.js
│   ├── style.css
│   └── favicon.svg
├── vercel.json
├── .github/workflows/ci.yml
├── .editorconfig
└── .gitignore
```

## Build and test

```sh
make            # build the binary and the test runner
make test       # run the test suite
make clean      # remove build artefacts
```

The CI matrix runs the build on Linux + macOS × `{gcc, clang}`
on every push and pull request.

## Contributing

Issues and pull requests welcome. See [`CONTRIBUTING.md`](CONTRIBUTING.md)
for the workflow. Bug reports should include:

- The platform and compiler (`uname -a; cc --version`)
- A minimal reproducer
- Expected vs actual behavior

## Security

See [`SECURITY.md`](SECURITY.md) for the supported versions
and how to report a vulnerability.

## License

MIT. See [`LICENSE`](LICENSE).

## Related projects

One of the
**[gpu-engineering curriculum](https://github.com/404Piyush/gpu-engineering)**,
a three-year systems and hardware roadmap. More projects
incoming.

Other projects so far:

- **[bst-library](https://github.com/404Piyush/bst-library)** — generic
  binary-search tree in C11 (week 4)
- **[arena-allocator](https://github.com/404Piyush/arena-allocator)** — bump
  arena memory allocator in C11 (week 8)

## Changelog

See [`CHANGELOG.md`](CHANGELOG.md). Format follows
[Keep a Changelog](https://keepachangelog.com/).
