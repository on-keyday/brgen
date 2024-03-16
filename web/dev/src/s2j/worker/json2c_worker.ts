import * as json2c from "../../lib/json2c.js";
import { JobRequest, RequestLanguage } from "../msg.js";
import { EmWorkContext} from "../em_work_ctx.js";
import { MyEmscriptenModule } from "../emscripten_mod.js";


const json2cModule = json2c.default as EmscriptenModuleFactory<MyEmscriptenModule>;


const requestCallback = (e:JobRequest, m:MyEmscriptenModule) => {
    switch (e.lang) {
        case RequestLanguage.C:
            if (e.sourceCode === undefined) return new Error("sourceCode is undefined");
            m.FS.writeFile("/editor.json", e.sourceCode);
            return ["json2c", "/editor.json", "--no-color"];
        default:
            return new Error("unknown message type");
    }
}


const j2c_ctx = new EmWorkContext(json2cModule,requestCallback, () => {
    console.log("json2c worker is ready");
});