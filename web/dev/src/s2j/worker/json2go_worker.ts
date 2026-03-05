/// <reference types="emscripten" />

export { };

import type { JobRequest} from "../msg.js";
import { RequestLanguage } from "../msg.js";
import { GoWorkContext} from "../go_work_ctx.js";
import { fetchOrReadWasm } from "./fetch_or_read.js";




const requestCallback = (e:JobRequest) => {
    switch (e.lang) {
        case RequestLanguage.GO:
            return e.sourceCode;
        default:
            return new Error("unknown message type");
    }
}



const j2go_ctx = new GoWorkContext(
    fetchOrReadWasm(new URL("../../lib/json2go.wasm",import.meta.url)),
    requestCallback, () => {
    console.log("json2go worker is ready");
});