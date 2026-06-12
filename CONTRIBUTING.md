# Contributing to pipe-shell

Issues, bug reports, and pull requests are welcome.

## Code of conduct

This project follows the [Contributor Covenant](CODE_OF_CONDUCT.md).
By participating you agree to its terms.

## Reporting a bug

Open a [bug report issue](../../issues/new?template=bug_report.md).
Include:

- Platform (`uname -a`) and compiler (`cc --version`)
- A minimal reproducer (the smallest command line that
  demonstrates the bug, plus the expected vs actual output)
- Stack trace if the shell crashed

## Proposing a change

Open a [feature request](../../issues/new?template=feature_request.md)
first. Discuss the design before writing code. Smaller changes
(a bug fix, a documentation typo) can go straight to a PR.

## Pull request workflow

1. Fork the repository.
2. Create a branch: `git checkout -b fix/short-description`.
3. Make your change.
4. Run `make test` locally. All 56 assertions must pass.
5. Run `make clean && make` with `-Wall -Wextra -Wpedantic`
   and confirm zero warnings.
6. Push to your fork and open a pull request against `main`.

PRs that touch `src/shell.c` or `include/shell.h` should:

- Add or update tests in `tests/test_shell.c`
- Update the relevant section of `docs/API.md` if the public
  API changes
- Add an entry under `[Unreleased]` in `CHANGELOG.md`

PRs that touch only documentation don't need a new test,
but should still pass the test suite unchanged.

## Coding style

- C11, no extensions.
- 4-space indent, no tabs.
- Lines under 80 columns where possible.
- One statement per line, no comma operators.
- `static` for everything not in the public API.
- Public types and functions prefixed `shell_`.
- Internal helpers unprefixed, marked `static`.

## Commit messages

One short line (50 chars or fewer) for the subject, blank line,
then a wrapped body explaining the *why*:

```
Fix strtok drop of empty trailing stages

strtok_r() silently returns NULL when the next field
after a delimiter is empty, which means 'ls |' parsed
as a single stage.  Switch to an explicit strchr loop
that rejects empty stages with a parse error.
```

## License

By contributing, you agree that your contributions will be
licensed under the MIT License. See [`LICENSE`](LICENSE).
