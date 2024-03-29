/// <reference types="emscripten" />

import * as json2ts from "../../lib/json2ts.js";
import { JobRequest, RequestLanguage } from "../msg.js";
import { EmWorkContext} from "../em_work_ctx.js";
import { MyEmscriptenModule } from "../emscripten_mod.js";


const json2tsModule = json2ts.default as EmscriptenModuleFactory<MyEmscriptenModule>;


const requestCallback = (e:JobRequest, m:MyEmscriptenModule) => {
    switch (e.lang) {
        case RequestLanguage.TYPESCRIPT:
            m.FS.writeFile("/editor.json", e.sourceCode);
            return ["json2ts", "/editor.json", "--no-color"];
        default:
            return new Error("unknown message type");
    }
};

const j2ts_ctx = new EmWorkContext(json2tsModule,requestCallback, () => {
    console.log("json2ts worker is ready");
});

