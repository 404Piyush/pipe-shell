/* app.js — pipe-shell playground.
 *
 * Live parser. As you type a command line, the AST and the
 * kernel-call trace update in real time. Click an example
 * to load it.
 */
'use strict';

const elInput   = document.getElementById('pg-input');
const elAst     = document.getElementById('pg-ast');
const elTrace   = document.getElementById('pg-trace');
const elExamples = document.querySelectorAll('.pg-example');

function renderAst(cmd) {
  if (cmd.error) {
    elAst.innerHTML = '<span style="color:#9a3412">parse error</span>';
    elTrace.textContent = '/* no trace */';
    return;
  }
  let html = '';
  for (let i = 0; i < cmd.stages.length; i++) {
    const s = cmd.stages[i];
    html += '<div class="pg-stage">';
    html += '<span class="pg-argv">';
    html += s.argv.map(a => escapeHtml(a)).join(' ');
    html += '</span>';
    if (s.infile)  html += `<span class="pg-redir">&lt; ${escapeHtml(s.infile)}</span>`;
    if (s.outfile) html += `<span class="pg-redir">${s.append ? '&gt;&gt;' : '&gt;'} ${escapeHtml(s.outfile)}</span>`;
    html += '</div>';
    if (i + 1 < cmd.stages.length) html += '<span class="pg-arrow"> | </span>';
  }
  if (cmd.background) html += ' <span style="color:var(--muted); font-size:11px;">&amp;</span>';
  elAst.innerHTML = html;
  elTrace.textContent = shellLib.kernelTrace(cmd);
}

function escapeHtml(s) {
  return String(s).replace(/[&<>"']/g, c => ({
    '&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;',
  }[c]));
}

function update() {
  const line = elInput.value;
  const cmd = shellLib.parse(line);
  renderAst(cmd);
}

elInput.addEventListener('input', update);

elExamples.forEach(b => {
  b.addEventListener('click', () => {
    elInput.value = b.dataset.cmd;
    update();
    elInput.focus();
  });
});

/* Initial state: a representative 3-stage pipeline. */
elInput.value = 'ls /etc | grep hosts | wc -l';
update();
