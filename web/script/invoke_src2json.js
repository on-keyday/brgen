/// <reference types="emscripten" />
import * as src2json from "./lib/src2json.js";
import { RequestMessage } from "./msg.js";
const Module = src2json.default;
const msgQueue = [];
(async () => {
    globalThis.onmessage = (ev) => {
        console.time(`msg queueing ${ev.data.jobID}`);
        msgQueue.push(ev.data);
    };
    const textCapture = {
        stdout: "",
        stderr: "",
    };
    const onInitalized = (mod) => {
        const emscripten_main = mod.cwrap("emscripten_main", "number", ["string"]);
        const callSrc2JSON = (args) => {
            let arg = "";
            for (let i = 0; i < args.length; i++) {
                if (i !== 0)
                    arg += " ";
                arg += encodeURIComponent(args[i]);
            }
            return emscripten_main(arg);
        };
        const handleMessage = (data) => {
            const e = data;
            if (e.msg === RequestMessage.MSG_SOURCE_CODE) {
                const id = e.jobID;
                textCapture.stdout = "";
                textCapture.stderr = "";
                const code = callSrc2JSON([
                    "src2json",
                    "--argv",
                    e.sourceCode,
                    "--no-color",
                    "--print-json",
                    "--print-on-error",
                ]);
                const result = {
                    stdout: textCapture.stdout,
                    stderr: textCapture.stderr,
                    code,
                    jobID: id,
                };
                postMessage(result);
            }
        };
        const handlingWithTimer = (job) => {
            console.time(`msg handling ${job.jobID}`);
            handleMessage(job);
            console.timeEnd(`msg handling ${job.jobID}`);
        };
        for (let i = 0; i < msgQueue.length; i++) {
            console.timeEnd(`msg queueing ${msgQueue[i].jobID}`);
            handlingWithTimer(msgQueue[i]);
        }
        globalThis.onmessage = (ev) => {
            handleMessage(ev.data);
        };
    };
    const mod = Module({
        print: (text) => {
            textCapture.stdout += text;
        },
        printErr: (text) => {
            textCapture.stderr += text;
        },
    }).then((mod) => {
        console.log("src2json worker initialized");
        onInitalized(mod);
        return mod;
    });
    globalThis.setTimeout(() => {
        console.log(mod);
    }, 10);
})();
//# sourceMappingURL=invoke_src2json.js.map