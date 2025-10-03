import * as ebmgen  from "../../../lib/bmgen/ebmgen.js";
import { JobRequest, RequestLanguage } from "../../msg.js";
import { EmWorkContext} from "../../em_work_ctx.js";
import { MyEmscriptenModule } from "../../emscripten_mod.js";

const ebmgenModule = ebmgen.default as EmscriptenModuleFactory<MyEmscriptenModule>;

const requestCallback = (e:JobRequest, m:MyEmscriptenModule) => {
    switch (e.lang) {
        case RequestLanguage.EBM:
            m.FS.writeFile("/editor.json", e.sourceCode);
            return ["ebmgen","-i", "/editor.json"];
        default:
            return new Error("unknown message type");
    }
}

const ebmgenWorker = new EmWorkContext(ebmgenModule,requestCallback, () => {
    console.log("ebmgen worker is ready");
});

