/// <reference types="emscripten" />

import * as json2cpp from "../../lib/json2cpp.js";
import { JobRequest, RequestLanguage } from "../msg.js";
import { EmWorkContext} from "../em_work_ctx.js";
import { MyEmscriptenModule } from "../emscripten_mod.js";


const json2cppModule = json2cpp.default as EmscriptenModuleFactory<MyEmscriptenModule>;


const requestCallback = (e:JobRequest, m:MyEmscriptenModule) => {
    switch (e.lang) {
        case RequestLanguage.CPP_PROTOTYPE:
            if (e.sourceCode === undefined) return new Error("sourceCode is undefined");
            m.FS.writeFile("/editor.json", e.sourceCode);
            return ["json2cpp", "/editor.json", "--no-color"];
        default:
            return new Error("unknown message type");
    }
};

const j2c_ctx = new EmWorkContext(json2cppModule,requestCallback, () => {
    console.log("json2cpp worker is ready");
});

