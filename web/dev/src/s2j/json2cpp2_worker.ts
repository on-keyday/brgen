import * as json2cpp2 from "../lib/json2cpp2.js";
import { JobRequest, RequestLanguage } from "./msg.js";
import { EmWorkContext, MyEmscriptenModule} from "./work_ctx.js";

const json2cpp2Module = json2cpp2.default as EmscriptenModuleFactory<MyEmscriptenModule>;
const j2c2_ctx = new EmWorkContext(json2cpp2Module, () => {
    console.log("json2cpp2 worker is ready");
});

setInterval(() => {
    j2c2_ctx.handleRequest((e, m) => {
        switch (e.lang) {
            case RequestLanguage.CPP:
                if (e.sourceCode === undefined) return new Error("sourceCode is undefined");
                m.FS.writeFile("/editor.json", e.sourceCode);
                return ["json2cpp2", "/editor.json", "--no-color"];
            default:
                return new Error("unknown message type");
        }
    });
    //j2c2_ctx.handleResponse();
}, 100);

globalThis.onmessage = (ev) => {
    const data = ev.data as JobRequest;
    j2c2_ctx.postRequest(data);
};