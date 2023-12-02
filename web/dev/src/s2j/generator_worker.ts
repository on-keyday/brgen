
/// <reference types="emscripten" />

export { };

import * as json2cpp from "../lib/json2cpp.js";
import * as json2cpp2 from "../lib/json2cpp2.js"
import { JobRequest, RequestLanguage } from "./msg.js";
import { EmWorkContext, GoWorkContext, MyEmscriptenModule} from "./work_ctx.js";

const json2cppModule = json2cpp.default as EmscriptenModuleFactory<MyEmscriptenModule>;
const j2c_ctx = new EmWorkContext(json2cppModule,() => {
    console.log("json2cpp worker is ready")
});

const json2cpp2Module = json2cpp2.default as EmscriptenModuleFactory<MyEmscriptenModule>;
const j2c2_ctx = new EmWorkContext(json2cpp2Module,() => {
    console.log("json2cpp2 worker is ready")
});

const j2go_ctx = new GoWorkContext(fetch("./json2go.wasm").then((r) => r.arrayBuffer()),()=>{
    console.log("json2go worker is ready")
});

setInterval(()=>{
    j2c_ctx.handleRequest((e,m) => {
        switch(e.lang) {
            case RequestLanguage.CPP_PROTOTYPE:
                if(e.sourceCode === undefined) return new Error("sourceCode is undefined");
                m.FS.writeFile("/editor.json",e.sourceCode);
                return ["json2cpp","/editor.json","--no-color"];
            default:
                return new Error("unknown message type");
        }
    });
    j2c2_ctx.handleRequest((e,m) => {
        switch(e.lang) {
            case RequestLanguage.CPP:
                if(e.sourceCode === undefined) return new Error("sourceCode is undefined");
                m.FS.writeFile("/editor.json",e.sourceCode);
                return ["json2cpp2","/editor.json","--no-color"];
            default:
                return new Error("unknown message type");
        }
    });
    j2go_ctx.handleRequest((e) => {
        switch(e.lang) {
            case RequestLanguage.GO:
                if(e.sourceCode === undefined) return new Error("sourceCode is undefined");
                return e.sourceCode
            default:
                return new Error("unknown message type");
        }
    });
},100);

setInterval(()=>{
    j2c_ctx.handleResponse();
    j2c2_ctx.handleResponse();
    j2go_ctx.handleResponse();
},100);

globalThis.onmessage = (ev) => {
    const data = ev.data as JobRequest;
    switch(data.lang) {
        case RequestLanguage.CPP_PROTOTYPE:
            j2c_ctx.postRequest(data);
            return;
        case RequestLanguage.CPP:
            j2c2_ctx.postRequest(data);
            return;
        case RequestLanguage.GO:
            j2go_ctx.postRequest(data);
            return;
    }
};
