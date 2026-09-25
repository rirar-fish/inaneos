// Driver for tools/wasm_parity.mjs: boots web/inaneos.wasm in a worker
// thread with the same shared-memory key protocol as web/worker.js.
// Interaction happens through the posted SharedArrayBuffer; no
// postMessage traffic is needed while the wasm thread runs.
const { parentPort, workerData } = require('worker_threads');
const fs = require('fs');

const disk = workerData.diskPath && fs.existsSync(workerData.diskPath)
  ? new Uint8Array(fs.readFileSync(workerData.diskPath))
  : null;

let memory = null;
let flag = null;
const imports = {
  env: {
    memory: null,
    key_wait(s) {
      if (flag) Atomics.wait(flag, 0, s, 1000);
    },
    disk_read(lba, dst) {
      if (!disk) return -1;
      const o = Number(lba) * 512;
      if (o + 512 > disk.length) return -1;
      new Uint8Array(memory.buffer).set(disk.subarray(o, o + 512), Number(dst));
      return 0;
    },
    disk_write(lba, src) {
      if (!disk) return -1;
      const o = Number(lba) * 512;
      if (o + 512 > disk.length) return -1;
      disk.set(new Uint8Array(memory.buffer).subarray(Number(src), Number(src) + 512), o);
      return 0;
    },
    disk_sectors() { return disk ? disk.length / 512 : 0; },
  },
};

(async () => {
  try {
    const bytes = fs.readFileSync(workerData.wasmPath);
    memory = new WebAssembly.Memory({ initial: 256, maximum: 4096, shared: true });
    imports.env.memory = memory;
    const { instance } = await WebAssembly.instantiate(bytes, imports);
    const w = instance.exports;
    flag = new Int32Array(memory.buffer, w.keyq_ptr() + 8, 1);
    parentPort.postMessage({ t: 'ready', mem: memory.buffer, cellsPtr: w.term_cells(), keyqPtr: w.keyq_ptr(), disk: !!disk });
    let enter = () => w.kernel_main(); // first boot inits, then shell
    for (;;) {
      try {
        enter();
        parentPort.postMessage({ t: 'error', msg: 'shell returned' });
        return;
      } catch (e) {
        if (e instanceof WebAssembly.RuntimeError) {
          if (w.halt_reason_get() === 3) {
            enter = () => w.enter_shell(); // sys_exit: fresh shell entry
            continue;
          }
          parentPort.postMessage({ t: 'halted', reason: w.halt_reason_get() });
          return;
        }
        parentPort.postMessage({ t: 'error', msg: String(e) });
        return;
      }
    }
  } catch (e) {
    parentPort.postMessage({ t: 'error', msg: String(e) });
  }
})();
