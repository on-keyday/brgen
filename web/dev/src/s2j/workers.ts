import { WorkerType } from "./msg";

export const fixedWorkerMap = Object.freeze({
    [WorkerType.SRC2JSON]:()=> new Worker(new URL("./worker/src2json_worker.js",import.meta.url),{type:"module"}),
    [WorkerType.JSON2CPP2]:()=> new Worker(new URL("./worker/json2cpp2_worker.js",import.meta.url),{type:"module"}),
    [WorkerType.JSON2GO]:()=> new Worker(new URL("./worker/json2go_worker.js",import.meta.url),{type:"module"}),
    [WorkerType.JSON2C]:()=> new Worker(new URL("./worker/json2c_worker.js",import.meta.url),{type:"module"}),
    [WorkerType.JSON2RUST]:()=> new Worker(new URL("./worker/json2rust_worker.js",import.meta.url),{type:"module"}),
    [WorkerType.JSON2TS]:()=> new Worker(new URL("./worker/json2ts_worker.js",import.meta.url),{type:"module"}),
    [WorkerType.JSON2KAITAI]:()=> new Worker(new URL("./worker/json2kaitai_worker.js",import.meta.url),{type:"module"}),

    [WorkerType.BMGEN]:()=> new Worker(new URL("./worker/bmgen/bmgen_worker.js",import.meta.url),{type:"module"}),
    [WorkerType.EBMGEN]:()=> new Worker(new URL("./worker/bmgen/ebmgen_worker.js",import.meta.url),{type:"module"}),
    //[WorkerType.BM2CPP]:()=> new Worker(new URL("./worker/bmgen/bm2cpp_worker.js",import.meta.url),{type:"module"}),
    //[WorkerType.BM2RUST]:()=> new Worker(new URL("./worker/bmgen/bm2rust_worker.js",import.meta.url),{type:"module"}),
});
