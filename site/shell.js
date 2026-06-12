/* shell.js — JavaScript port of the pipe-shell parser.
 *
 * Same surface as the C version:
 *     shell_parse(line, out)
 *
 * The output is a list of stages; each stage has argv,
 * optional infile / outfile / append.
 *
 * Used by the playground to render the AST and the kernel
 * call sequence. The executor itself is not ported; the
 * playground visualizes the kernel calls instead.
 */
'use strict';

const SHELL_MAX_TOKENS = 64;
const SHELL_MAX_CMDS   = 8;

function isSpace(c) { return c === ' ' || c === '\t' || c === '\n'; }

function tokenizeStage(s) {
  const argv = [];
  let infile = null, outfile = null, append = false;
  let n = 0;
  let i = 0;
  while (i < s.length) {
    while (i < s.length && isSpace(s[i])) i++;
    if (i >= s.length) break;
    if (n >= SHELL_MAX_TOKENS - 1) return { error: true };

    /* Redirections. */
    if (s[i] === '<') {
      i++;
      while (i < s.length && isSpace(s[i])) i++;
      const start = i;
      while (i < s.length && !isSpace(s[i])) i++;
      infile = s.slice(start, i);
      continue;
    }
    if (s[i] === '>' && s[i + 1] === '>') {
      append = true; i += 2;
      while (i < s.length && isSpace(s[i])) i++;
      const start = i;
      while (i < s.length && !isSpace(s[i])) i++;
      outfile = s.slice(start, i);
      continue;
    }
    if (s[i] === '>') {
      i++;
      while (i < s.length && isSpace(s[i])) i++;
      const start = i;
      while (i < s.length && !isSpace(s[i])) i++;
      outfile = s.slice(start, i);
      continue;
    }
    /* Ordinary word. */
    const start = i;
    while (i < s.length && !isSpace(s[i]) && s[i] !== '<' && s[i] !== '>') i++;
    argv.push(s.slice(start, i));
    n++;
  }
  argv.push(null);
  if (n === 0) return { error: true };
  return { argv: argv.slice(0, -1), infile, outfile, append };
}

function shell_parse(line) {
  /* Strip trailing `&` (background). */
  let background = false;
  const trimmed = line.replace(/\s+$/, '');
  if (trimmed.endsWith('&')) {
    background = true;
    line = trimmed.slice(0, -1);
  }
  /* Empty line. */
  if (line.replace(/\s+/g, '') === '') return { error: true };

  /* Split on `|`. */
  const stages = [];
  let rest = line;
  while (rest !== null) {
    const bar = rest.indexOf('|');
    let stageStr;
    if (bar === -1) { stageStr = rest; rest = null; }
    else { stageStr = rest.slice(0, bar); rest = rest.slice(bar + 1); }
    stageStr = stageStr.replace(/^\s+|\s+$/g, '');
    const tok = tokenizeStage(stageStr);
    if (tok.error) return { error: true };
    stages.push(tok);
  }
  return { stages, background, nStages: stages.length };
}

/* Synthesize the kernel call sequence that an executor would make
 * for the parsed command.  We do not actually fork; this is a
 * visual aid that walks through what the kernel would do. */
function kernelTrace(cmd) {
  const lines = [];
  lines.push('/* parent (shell) */');
  for (let i = 0; i < cmd.nStages - 1; i++) {
    lines.push(`  pipe(fd[${i}])  ->  read=fd[${i}][0]  write=fd[${i}][1]`);
  }
  for (let i = 0; i < cmd.nStages; i++) {
    const pid = 1000 + i;
    lines.push(`  fork()  ->  child pid=${pid}`);
    const st = cmd.stages[i];
    if (st.infile) lines.push(`    [pid ${pid}] open("${st.infile}")  ->  dup2(fd, 0)`);
    else if (i > 0) lines.push(`    [pid ${pid}] dup2(fd[${i-1}][0], 0)`);
    if (st.outfile) {
      const mode = st.append ? 'O_WRONLY|O_CREAT|O_APPEND' : 'O_WRONLY|O_CREAT|O_TRUNC';
      lines.push(`    [pid ${pid}] open("${st.outfile}", ${mode})  ->  dup2(fd, 1)`);
    } else if (i + 1 < cmd.nStages) {
      lines.push(`    [pid ${pid}] dup2(fd[${i}][1], 1)`);
    }
    lines.push(`    [pid ${pid}] close(...)  /* unused fds */`);
    lines.push(`    [pid ${pid}] execvp("${st.argv[0]}", argv)`);
  }
  if (!cmd.background) {
    for (let i = 0; i < cmd.nStages; i++) {
      lines.push(`  waitpid(pid=${1000+i}, &st, 0)`);
    }
  } else {
    lines.push('  /* background: parent does not wait */');
  }
  return lines.join('\n');
}

window.shellLib = { parse: shell_parse, kernelTrace };
