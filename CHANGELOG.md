# Changelog

All notable changes to pipe-shell are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [1.0.0] - 2026-06-12

### Added
- Interactive editorial landing page at
  [pipe-shell.404piyush.me](https://pipe-shell.404piyush.me)
  with a JavaScript port of the parser and a live AST
  visualiser with kernel-call tracing.
- `CHANGELOG.md` (this file).
- `CONTRIBUTING.md`.
- `CODE_OF_CONDUCT.md` (Contributor Covenant 2.1).
- `SECURITY.md` with a hardening section for untrusted
  input.
- `SHOWCASE.md` with parser walkthrough, executor
  walkthrough, and a per-stage syscall trace of a
  3-stage pipeline.
- `.github/ISSUE_TEMPLATE/` (bug report, feature request,
  documentation issue).
- `.github/PULL_REQUEST_TEMPLATE.md`.

### Notes
- Source under MIT.
- 56 assertions, 13 test cases, all passing.
- CI on Linux + macOS × {clang, gcc}, all green.

## [0.1.0] - 2026-06-11

### Added
- Initial release: `shell_parse`, `shell_run`,
  `shell_cmd_print` and the `shell_cmd` / `shell_stage`
  types.
- Recursive parser: line → AST.
- Executor: N-1 pipes, N children, N-1 parent-side
  `close()`s.
- Redirection: `<`, `>`, `>>`.
- Background: trailing `&`.
- Built-ins: `exit [N]`, `cd [DIR]`.
- Interactive REPL and one-shot `--run` CLI.
- Test suite: 13 tests, 56 assertions, 0 failures.
- CI on Linux + macOS × {clang, gcc}.
- MIT license.
- Full API and architecture docs in `docs/`.

### Known limitations
- No glob (`*`) or variable (`$HOME`) expansion.
- No quoting of any kind.
- No `;`, `&&`, `||`. A line is a single pipeline.
- No job control.
- Single-threaded shell; children are the OS's problem.
