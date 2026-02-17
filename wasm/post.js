// Web Worker message handler (skipped in Node.js)
// Send UCI command strings via postMessage to control the engine.
//
// Example from main thread:
//   var worker = new Worker('cwtch.js');
//   worker.onmessage = function(e) { console.log(e.data); };
//   worker.postMessage('uci');
//   worker.postMessage('ucinewgame');
//   worker.postMessage('position startpos');
//   worker.postMessage('go depth 10');

if (!_isNode) {
  self.onmessage = function(e) {

    var cmd = e.data;

    if (typeof cmd !== 'string')
      return;

    // Marshal the JS string into WASM linear memory
    var len = Module.lengthBytesUTF8(cmd) + 1;
    var ptr = Module._malloc(len);
    Module.stringToUTF8(cmd, ptr, len);

    Module._wasm_exec(ptr);

    Module._free(ptr);

  };
}
