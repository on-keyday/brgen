/// <reference types="emscripten" />

export { };

import * as src2json from "../../lib/src2json.js";
import { RequestLanguage, JobRequest } from "../msg.js";
import { EmWorkContext} from "../em_work_ctx.js";
import { MyEmscriptenModule } from "../emscripten_mod.js";
import { fetchImportFile } from "../dummy_fs.js";



const src2jsonModule = src2json.default as EmscriptenModuleFactory<MyEmscriptenModule>;


const requestCallback = async (e:JobRequest, m:MyEmscriptenModule) => {
    switch(e.lang) {
        case RequestLanguage.JSON_AST:
            await fetchImportFile(m,e.sourceCode);
            return ["src2json","--argv",e.sourceCode,"--no-color","--print-json","--print-on-error"];
        case RequestLanguage.JSON_DEBUG_AST:
            await fetchImportFile(m,e.sourceCode);
            return ["src2json","--argv",e.sourceCode,"--no-color","--print-json","--print-on-error","--debug-json"];
        case RequestLanguage.TOKENIZE:
            return ["src2json","--argv",e.sourceCode,"--no-color","--print-json","--print-on-error","--lexer"];
        default:
            return new Error("unknown message type");
    }
}

const ctx = new EmWorkContext(src2jsonModule,requestCallback,() => {
    console.log("src2json worker is ready")
});


