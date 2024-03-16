import * as json2cpp2 from "../../lib/json2cpp2.js";
import { JobRequest, RequestLanguage } from "../msg.js";
import { EmWorkContext} from "../em_work_ctx.js";
import { MyEmscriptenModule } from "../emscripten_mod.js";

const json2cpp2Module = json2cpp2.default as EmscriptenModuleFactory<MyEmscriptenModule>;

const requestCallback = (e:JobRequest, m:MyEmscriptenModule) => {
    switch (e.lang) {
        case RequestLanguage.CPP:
            if (e.sourceCode === undefined) return new Error("sourceCode is undefined");
            m.FS.writeFile("/editor.json", e.sourceCode);
            return ["json2cpp2", "/editor.json", "--no-color"];
        default:
            return new Error("unknown message type");
    }
}

const j2c2_ctx = new EmWorkContext(json2cpp2Module,requestCallback, () => {
    console.log("json2cpp2 worker is ready");
});

