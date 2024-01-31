/// <reference types="emscripten" />

export { };

import { JobRequest, RequestLanguage } from "./msg.js";
import { GoWorkContext} from "./work_ctx.js";


const j2go_ctx = new GoWorkContext(fetch(new URL("../lib/json2go.wasm",import.meta.url)).then((r) => r.arrayBuffer()), () => {
    console.log("json2go worker is ready");
});

const requestCallback = (e:JobRequest) => {
    switch (e.lang) {
        case RequestLanguage.GO:
            if (e.sourceCode === undefined) return new Error("sourceCode is undefined");
            return e.sourceCode;
        default:
            return new Error("unknown message type");
    }
}

setInterval(() => {
    j2go_ctx.handleRequest(requestCallback);
}, 100);

globalThis.onmessage = (ev) => {
    const data = ev.data as JobRequest;
    switch (data.lang) {
        case RequestLanguage.GO:
            j2go_ctx.postRequest(data);
            return;
    }
};
