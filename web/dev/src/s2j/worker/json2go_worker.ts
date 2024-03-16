/// <reference types="emscripten" />

export { };

import { JobRequest, RequestLanguage } from "../msg.js";
import { GoWorkContext} from "../go_work_ctx.js";




const requestCallback = (e:JobRequest) => {
    switch (e.lang) {
        case RequestLanguage.GO:
            if (e.sourceCode === undefined) return new Error("sourceCode is undefined");
            return e.sourceCode;
        default:
            return new Error("unknown message type");
    }
}


const j2go_ctx = new GoWorkContext(
    fetch(new URL("../../lib/json2go.wasm",import.meta.url)).then((r) => r.arrayBuffer()),
    requestCallback, () => {
    console.log("json2go worker is ready");
});