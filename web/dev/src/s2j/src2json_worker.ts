/// <reference types="emscripten" />

export { };

import * as src2json from "../lib/src2json.js";
import { RequestMessage, JobRequest } from "./msg.js";
import { EmWorkContext, MyEmscriptenModule} from "./work_ctx.js";



const src2jsonModule = src2json.default as EmscriptenModuleFactory<MyEmscriptenModule>;
const ctx = new EmWorkContext(src2jsonModule,() => {
    console.log("src2json worker is ready")
});

setInterval(()=>{
    ctx.handleRequest((e) => {
        switch(e.msg) {
            case RequestMessage.MSG_REQUIRE_AST:
                if(e.sourceCode === undefined) return new Error("sourceCode is undefined");
                return ["src2json","--argv",e.sourceCode,"--no-color","--print-json","--print-on-error"];
            case RequestMessage.MSG_REQUIRE_TOKENS:
                if(e.sourceCode === undefined) return new Error("sourceCode is undefined");
                return ["src2json","--argv",e.sourceCode,"--no-color","--print-tokens","--print-on-error","--lexer"];
            default:
                return new Error("unknown message type");
        }
    });
},100);

setInterval(()=>{
    ctx.handleResponse();
},100);

globalThis.onmessage = (ev) => {
    ctx.postRequest(ev.data as JobRequest);
};


