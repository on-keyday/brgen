import * as bmgen  from "../../../lib/bmgen/bmgen.js";
import type { JobRequest} from "../../msg.js";
import { RequestLanguage } from "../../msg.js";
import { EmWorkContext} from "../../em_work_ctx.js";
import type { MyEmscriptenModule } from "../../emscripten_mod.js";

const bmgenModule = bmgen.default as EmscriptenModuleFactory<MyEmscriptenModule>;

const requestCallback = (e:JobRequest, m:MyEmscriptenModule) => {
    switch (e.lang) {
        case RequestLanguage.BINARY_MODULE:
            m.FS.writeFile("/editor.json", e.sourceCode);
            if(e.arguments?.includes("--print-instructions")) {
                return ["bmgen","-i", "/editor.json"];
            }
            else {
                return ["bmgen","-i", "/editor.json","--base64","-o","-"];
            }
        default:
            return new Error("unknown message type");
    }
}

const bmgenWorker = new EmWorkContext(bmgenModule,requestCallback, () => {
    console.log("bmgen worker is ready");
});

