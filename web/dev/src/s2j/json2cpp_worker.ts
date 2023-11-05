
/// <reference types="emscripten" />

export { };

import * as json2cpp from "../lib/json2cpp.js";
import { RequestMessage, JobRequest } from "./msg.js";
import { EmWorkContext, MyEmscriptenModule} from "./work_ctx.js";



const json2cppModule = json2cpp.default as EmscriptenModuleFactory<MyEmscriptenModule>;
const ctx = new EmWorkContext(json2cppModule,() => {
    console.log("json2cpp worker is ready")
});

setInterval(()=>{
    ctx.handleRequest((e,m) => {
        switch(e.msg) {
            case RequestMessage.MSG_REQUIRE_GENERATED_CODE:
                if(e.sourceCode === undefined) return new Error("sourceCode is undefined");
                m.FS.writeFile("/editor.json",e.sourceCode);
                return ["json2cpp","/editor.json"];
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
