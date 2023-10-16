/// <reference types="emscripten" />

export { };

import * as src2json from "../lib/src2json.js";
import { RequestMessage, JobRequest, JobResult } from "./msg.js";

interface MyEmscriptenModule extends EmscriptenModule {
    ccall: typeof ccall;
    cwrap: typeof cwrap;
}

const Module = src2json.default as EmscriptenModuleFactory<MyEmscriptenModule>;

const msgQueue: JobRequest[] = [];


(async () => {
    globalThis.onmessage = (ev) => {
        console.time(`msg queueing ${ev.data.jobID}`);
        msgQueue.push(ev.data);
    };
    const textCapture = {
        stdout: "",
        stderr: "",
    };

    const onInitalized = (mod: MyEmscriptenModule) => {
        const emscripten_main = mod.cwrap("emscripten_main", "number", ["string"]);

        const callSrc2JSON = (args: string[]): number => {
            let arg: string = "";
            for (let i = 0; i < args.length; i++) {
                if (i !== 0) arg += " ";
                arg += encodeURIComponent(args[i]);
            }
            return emscripten_main(arg);
        };

        const handleMessage = (data: JobRequest) => {
            const e = data ;
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
                const result: JobResult = {
                    stdout: textCapture.stdout,
                    stderr: textCapture.stderr,
                    code,
                    jobID: id,
                };
                postMessage(result);
            }
        };
        const handlingWithTimer = (job :JobRequest) => {
            console.time(`msg handling ${job.jobID}`)
            handleMessage(job);
            console.timeEnd(`msg handling ${job.jobID}`)
        };
        for(let i=0;i<msgQueue.length;i++){
            console.timeEnd(`msg queueing ${msgQueue[i].jobID}`);
            handlingWithTimer(msgQueue[i]);
        }
        globalThis.onmessage = (ev) => {
           handlingWithTimer(ev.data);
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
        console.log(mod)
    }, 10)
})();
