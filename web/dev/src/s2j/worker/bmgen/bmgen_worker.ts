import * as bmgen  from "../../../lib/bmgen/bmgen.js";
import { JobRequest, RequestLanguage } from "../../msg.js";
import { EmWorkContext} from "../../em_work_ctx.js";
import { MyEmscriptenModule } from "../../emscripten_mod.js";

const bmgenModule = bmgen.default as EmscriptenModuleFactory<MyEmscriptenModule>;

const requestCallback = (e:JobRequest, m:MyEmscriptenModule) => {
    switch (e.lang) {
        case RequestLanguage.BINARY_MODULE:
            m.FS.writeFile("/editor.json", e.sourceCode);
            if(e.arguments?.includes("print-instructions")) {
                return ["bmgen","-i", "/editor.json","--print-instructions"];
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

