/// <reference types="emscripten" />

export { };

import * as json2cpp from "../lib/json2cpp.js";
import { JobRequest, RequestLanguage } from "./msg.js";
import { EmWorkContext, MyEmscriptenModule} from "./work_ctx.js";


const json2cppModule = json2cpp.default as EmscriptenModuleFactory<MyEmscriptenModule>;
const j2c_ctx = new EmWorkContext(json2cppModule, () => {
    console.log("json2cpp worker is ready");
});

setInterval(() => {
    j2c_ctx.handleRequest((e, m) => {
        switch (e.lang) {
            case RequestLanguage.CPP_PROTOTYPE:
                if (e.sourceCode === undefined) return new Error("sourceCode is undefined");
                m.FS.writeFile("/editor.json", e.sourceCode);
                return ["json2cpp", "/editor.json", "--no-color"];
            default:
                return new Error("unknown message type");
        }
    });
    //j2c_ctx.handleResponse();
}, 100);

globalThis.onmessage = (ev) => {
    const data = ev.data as JobRequest;
    j2c_ctx.postRequest(data);
};