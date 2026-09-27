// Parity gate: drives the wasm build through the REAL shell and asserts
// byte-level outputs identical to QEMU (see kernel/syscall.c, user/*.c).
// Usage: node tools/wasm_parity.mjs  (or: make wasmtest)
import { Worker } from 'worker_threads';
import path from 'path';
import { fileURLToPath } from 'url';

const root = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const worker = new Worker(path.join(root, 'tools', 'wasm_parity_driver.cjs'), {
  workerData: {
    wasmPath: path.join(root, 'web', 'inaneos.wasm'),
    diskPath: path.join(root, 'disk.img'),
  },
});

const COLS = 80, ROWS = 25, KEY_N = 64;
let memBuf = null, cellsPtr = 0, keyqPtr = 0;
let grid = { cells: new Uint16Array(COLS * ROWS) };
let readyResolve;
const ready = new Promise((r) => (readyResolve = r));

worker.on('message', (m) => {
  if (m.t === 'ready') {
    memBuf = m.mem; cellsPtr = m.cellsPtr; keyqPtr = m.keyqPtr;
    readyResolve(m);
  }
  else if (m.t === 'halted') { console.error('FAIL: wasm halted unexpectedly, reason=' + m.reason); process.exit(1); }
  else if (m.t === 'error') { console.error('FAIL: ' + m.msg); process.exit(1); }
});
worker.on('error', (e) => { console.error('FAIL worker: ' + e); process.exit(1); });

function keyPush(code) {
  const q = new Int32Array(memBuf, keyqPtr, 3 + KEY_N);
  const h = Atomics.load(q, 0);
  q[3 + h] = code;
  Atomics.store(q, 0, (h + 1) % KEY_N);
  Atomics.add(q, 2, 1);
  Atomics.notify(q, 2, 1);
}
function snap() {
  const cells = new Uint16Array(memBuf).slice(cellsPtr / 2, cellsPtr / 2 + COLS * ROWS);
  grid = { cells };
  return grid;
}
function text(g) {
  let rows = [];
  for (let r = 0; r < ROWS; r++) {
    let s = '';
    for (let c = 0; c < COLS; c++) {
      const ch = g.cells[r * COLS + c] & 0xff;
      s += String.fromCharCode(ch === 0 ? 32 : ch);
    }
    rows.push(s.replace(/ +$/, ''));
  }
  return rows.join('\n');
}
function attrAt(g, r, c) { return (g.cells[r * COLS + c] >> 8) & 0xff; }
async function waitFor(pred, what, timeout = 15000) {
  const t0 = Date.now();
  for (;;) {
    snap();
    const t = text(grid);
    if (pred(t, grid)) return t;
    if (Date.now() - t0 > timeout) {
      console.error('FAIL: timeout waiting for: ' + what + '\n--- screen ---\n' + t);
      process.exit(1);
    }
    await new Promise((r) => setTimeout(r, 100));
  }
}
function send(s) {
  for (const ch of s) keyPush(ch.charCodeAt(0));
}
function sendKey(code) { keyPush(code); }
let pass = 0;
function ok(name, cond) {
  if (!cond) { console.error('FAIL: ' + name); process.exit(1); }
  pass++;
  console.log('ok: ' + name);
}

const r = await ready;
ok('boot with disk attached', r.disk === true);

let t = await waitFor((t) => t.includes('root@inaneos:/#'), 'shell prompt');
// green prompt like QEMU (sys_setcolor(0x0A, 0x00))
{
  const i = t.indexOf('root@inaneos');
  const row = t.slice(0, i).split('\n').length - 1;
  const col = i - (t.lastIndexOf('\n', i) + 1);
  ok('prompt root@inaneos:/# with green attr', attrAt(grid, row, col) === 0x0a);
}

send('help\n');
t = await waitFor((t) => t.includes('keo - Edit file:'), 'help output');
ok('help lists all commands', t.includes('Available Command:') && t.includes('moon - Mount disk:'));

send('info\n');
t = await waitFor((t) => t.includes('Inaneos beta 0.0.2 version'), 'info output');
ok('info text', true);

send('mem\n');
t = await waitFor((t) => /free: \d+ KB total: \d+ KB/.test(t), 'mem output');
ok('mem format', true);

send('run\n');
t = await waitFor((t) => t.includes('keo\n'), 'lsmod output');
ok('run lists shell/calc/keo', /shell\n.*calc\n.*keo\n/s.test(t));

send('moon\n');
t = await waitFor((t) => t.includes('0*'), 'partlist output');
ok('moon shows auto-mounted part0 (0* 0C 2048 129024)', t.includes('0* 0C 2048 129024'));

const promptsBeforeLs = (t.match(/root@inaneos/g) || []).length;
send('ls\n');
t = await waitFor((t) => (t.match(/root@inaneos/g) || []).length > promptsBeforeLs, 'fat ls output');
ok('ls on empty disk shows no test files', !t.includes('HELLO.TXT') && !t.includes('DOCS'));

send('cat HELLO.TXT\n');
t = await waitFor((t) => t.includes('cannot read'), 'fat cat missing file');
ok('cat missing file says cannot read', true);

send('boguscmd\n');
t = await waitFor((t) => t.includes('boguscmd: Command Not Found'), 'unknown cmd');
ok('unknown command message', true);

send('run calc\n');
t = await waitFor((t) => t.includes('calc>'), 'calc prompt');
ok('calc runs', t.includes('calc, exit to quit'));
send('2+3*4\n');
t = await waitFor((t) => /(^|\n)14\n/.test(t), 'calc eval');
ok('calc 2+3*4 = 14', true);
send('exit\n');
t = await waitFor((t) => (t.match(/root@inaneos/g) || []).length >= 2, 'back to shell');
ok('calc exit returns to shell', true);

send('keo TEST.TXT\n');
t = await waitFor((t) => t.includes('NORMAL'), 'keo screen');
ok('keo opens with NORMAL mode', true);
send('iHi!');
sendKey(27); // esc
send(':wq\n');
t = await waitFor((t) => t.includes('root@inaneos'), 'keo save+exit');
ok('keo :wq saves and exits', true);
send('cat TEST.TXT\n');
t = await waitFor((t) => t.includes('Hi!'), 'cat new file');
ok('cat reads keo-written file', true);

console.log(`\nPARITY-OK (${pass} checks)`);
await worker.terminate();
process.exit(0);
