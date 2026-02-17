// Cwtch unified harness - works in Node.js CLI and browser Web Workers.
// Detects environment automatically. Customize print/printErr to change output.

var _isNode = typeof process !== 'undefined' && process.versions && process.versions.node;

var _ready = false;
var _queue = [];

var Module = {

  // In Node, output goes to stdout/stderr.
  // In a Web Worker, output goes via postMessage.
  print: _isNode
    ? function(text) { console.log(text); }
    : function(text) { postMessage(text); },

  printErr: _isNode
    ? function(text) { console.error(text); }
    : function(text) { postMessage('stderr: ' + text); },

  // Called once the WASM engine is fully loaded
  onRuntimeInitialized: function() {
    if (_isNode) {
      var args = process.argv.slice(2);
      if (args.length > 0) {
        // CLI args: run them via main() and exit
        Module.callMain(args);
      }
      else {
        // Interactive mode: use wasm_init/wasm_exec with Node readline
        Module._wasm_init();
        var readline = require('readline');
        var rl = readline.createInterface({input: process.stdin, terminal: false});
        rl.on('line', function(line) {
          var len = Module.lengthBytesUTF8(line) + 1;
          var ptr = Module._malloc(len);
          Module.stringToUTF8(line, ptr, len);
          var ok = Module._wasm_exec(ptr);
          Module._free(ptr);
          if (!ok) rl.close();
        });
        rl.on('close', function() { process.exit(0); });
      }
    }
    else {
      Module._wasm_init();
      _ready = true;
      for (var i = 0; i < _queue.length; i++)
        self.onmessage({data: _queue[i]});
      _queue = [];
      postMessage('ready');
    }
  }

};
