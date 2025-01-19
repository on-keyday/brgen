import * as bmgen  from "../../../lib/bmgen/bm2rust.js";
import { JobRequest, RequestLanguage } from "../../msg.js";
import { EmWorkContext} from "../../em_work_ctx.js";
import { MyEmscriptenModule } from "../../emscripten_mod.js";
import { base64ToUint8Array } from "./util.js";

const bmgenModule = bmgen.default as EmscriptenModuleFactory<MyEmscriptenModule>;

const requestCallback = (e:JobRequest, m:MyEmscriptenModule) => {
    switch (e.lang) {
        case RequestLanguage.RUST_2:
            const bm = base64ToUint8Array(e.sourceCode);
            if(bm instanceof Error) {
                return bm;
            }
            m.FS.writeFile("/editor.bm",  bm);
            return ["bm2rust","-i", "/editor.bm"].concat(e.arguments || []);
        default:
            return new Error("unknown message type");
    }
}

const bmgenWorker = new EmWorkContext(bmgenModule,requestCallback, () => {
    console.log("bm2rust worker is ready");
});

