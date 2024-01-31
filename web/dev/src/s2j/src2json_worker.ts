/// <reference types="emscripten" />

export { };

import * as src2json from "../lib/src2json.js";
import { RequestLanguage, JobRequest } from "./msg.js";
import { EmWorkContext, MyEmscriptenModule} from "./work_ctx.js";



const src2jsonModule = src2json.default as EmscriptenModuleFactory<MyEmscriptenModule>;
const ctx = new EmWorkContext(src2jsonModule,() => {
    console.log("src2json worker is ready")
});

const requestCallback = (e:JobRequest, m:MyEmscriptenModule) => {
    switch(e.lang) {
        case RequestLanguage.JSON_AST:
            if(e.sourceCode === undefined) return new Error("sourceCode is undefined");
            return ["src2json","--argv",e.sourceCode,"--no-color","--print-json","--print-on-error"];
        case RequestLanguage.JSON_DEBUG_AST:
            if(e.sourceCode === undefined) return new Error("sourceCode is undefined");
            return ["src2json","--argv",e.sourceCode,"--no-color","--print-json","--print-on-error","--debug-json"];
        case RequestLanguage.TOKENIZE:
            if(e.sourceCode === undefined) return new Error("sourceCode is undefined");
            return ["src2json","--argv",e.sourceCode,"--no-color","--print-json","--print-on-error","--lexer"];
        default:
            return new Error("unknown message type");
    }
}

setInterval(()=>{
    ctx.handleRequest(requestCallback);
},100);


globalThis.onmessage = (ev) => {
    ctx.postRequest(ev.data as JobRequest);
};


