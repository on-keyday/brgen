const { copyFile } = require("fs");
const path = require("path");
const copyWasm = (p) => {
  copyFile(
    path.resolve(__dirname, "src/lib/", p),
    path.resolve(__dirname, "out/lib/", p),
    (err) => {
      if (err) {
        console.log(err);
      }
    }
  );
};

copyWasm("src2json.wasm");
//copyWasm("json2cpp.wasm");
copyWasm("json2cpp2.wasm");
copyWasm("json2go.wasm");
copyWasm("json2c.wasm");
copyWasm("json2ts.wasm");
copyWasm("json2kaitai.wasm");

// for rebrgen
copyWasm("bmgen/bmgen.wasm");
copyWasm("bmgen/bm2cpp.wasm");
copyWasm("bmgen/bm2rust.wasm");
