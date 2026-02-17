// Cwtch unified harness - works in Node.js CLI and browser Web Workers.
// Detects environment automatically. Customize print/printErr to change output.

var _isNode = typeof process !== 'undefined' && process.versions && process.versions.node;

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
      // Run main() with CLI args (or interactive stdin if no args)
      Module.callMain(process.argv.slice(2));
    }
    else {
      Module._wasm_init();
      postMessage('ready');
    }
  }

};
