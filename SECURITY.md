# Security

The pipe-shell binary is a small command interpreter. It
runs in the caller's process, parses untrusted input, and
forks child processes. The attack surface is therefore
larger than a pure data-structure library.

## Hardening

The shell does not, by design, support globbing, variable
expansion, or any form of quoting. Every word on the
command line is passed verbatim to `execvp`. If you need
to expose the shell to untrusted input, write a wrapper
that:

- Validates the input against a known syntax (e.g. only
  allow `program arg1 arg2`)
- Sets a tight `PATH` before exec
- Drops privileges before exec
- Runs in a chroot or container
- Rate-limits forks

The shell will not protect you from itself; it is a
library, not a sandbox.

## Supported versions

| Version | Supported           |
|---------|---------------------|
| 1.x.x   | Yes                 |
| < 1.0   | No                  |

## Reporting a vulnerability

If you find a security issue, please email
`piyushutkar123@gmail.com` rather than opening a public issue.
You can expect an acknowledgement within 48 hours and a
remediation plan within 7 days.

## Past advisories

None.
