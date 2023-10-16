/// <reference types="emscripten" />



export {};

import * as src2json from "./lib/src2json.js";

const Module = src2json.default as EmscriptenModuleFactory<MyEmscriptenModule>;

(async () => {
    const textCapture = {
        stdout : "",
        stderr : ""
    }
    const mod = await Module({
        print: (text) => {
            console.log(text)
            textCapture.stdout += text;
        },
        printErr: (text) => {
            console.error(text)
            textCapture.stderr += text;
        }
    })


    const emscripten_main = mod.cwrap("emscripten_main","number",["string"]);

    const callSrc2JSON = (args :string[]):number => {
        let arg :string = ""
        for(let i = 0; i < args.length; i++){
            if(i!==0) arg += " ";
            arg += encodeURIComponent(args[i]);
        }
        return emscripten_main(arg);
    }

    window.onmessage = (ev) => {
        const e = ev.data as JobRequest;
        if(e.msg===ReqestMessage.MSG_SOURCE_CODE){
            const id = e.jobID;
            textCapture.stdout = ""
            textCapture.stderr = ""
            const code = callSrc2JSON(["src2json","--argv", e.sourceCode,"--no-color","--print-json","--print-on-error"]);
            const result :JobResult = {
                stdout : textCapture.stdout,
                stderr : textCapture.stderr,
                code,
                jobID : id
            }
            postMessage(result);
        }
    }
})();