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
const postQueue: JobResult[] = [];

globalThis.onmessage = (ev) => {
    console.time(`msg queueing ${ev.data.jobID}`);
    msgQueue.push(ev.data);
};

setInterval(()=>{
    while(postQueue.length > 0){
        console.timeEnd(`msg posting ${postQueue[0].jobID}`);
        globalThis.postMessage(postQueue.shift() as JobResult);
    }
},100);

const doPost = (msg :JobResult) => {
    console.time(`msg posting ${msg.jobID}`);
    postQueue.push(msg);
}

const textCapture = {
    jobId: -1,
    stdout: "",
    stderr: "",
};

(async () => {
    


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

        const requireSourceCode = (msg :RequestMessage,jobID :number, code :string|undefined): code is string => {
            if(code===undefined){
                const res: JobResult = {
                    msg: msg,
                    jobID: jobID,
                    err: new Error("sourceCode is undefined"),
                    code: -1,
                }
                postMessage(res);
                return false;
            }
            return true;
        };

        const doExec = (e :JobRequest,args: string[]) => {
            const id = e.jobID;
            if(textCapture.jobId !== -1){
                const res: JobResult = {
                    msg: e.msg,
                    jobID: id,
                    err: new Error("previous job is not finished"),
                    code: -1,
                }
                doPost(res);
                return;
            }
            textCapture.jobId = id;
            textCapture.stdout = "";
            textCapture.stderr = "";
            const code = callSrc2JSON(args);
            const result: JobResult = {
                msg :e.msg,
                stdout: textCapture.stdout,
                stderr: textCapture.stderr,
                code,
                jobID: id,
            };
            textCapture.stdout = "";
            textCapture.stderr = "";
            textCapture.jobId = -1;
            doPost(result);
        }

        const handleMessage = (data: JobRequest) => {
            const e = data ;
            if (e.msg === RequestMessage.MSG_REQUIRE_AST) {
                if(!requireSourceCode(e.msg,e.jobID,e.sourceCode)) return;
                doExec(e,["src2json","--argv",e.sourceCode,"--no-color","--print-json","--print-on-error"]);
            }
            else if(e.msg===RequestMessage.MSG_REQUIRE_TOKENS) {
                if(!requireSourceCode(e.msg,e.jobID,e.sourceCode)) return;
                doExec(e,["src2json","--argv",e.sourceCode,"--no-color","--print-tokens","--print-on-error","--lexer"]);   
            }
            else{
                const errRes :JobResult = {
                    msg:e.msg,
                    jobID :e.jobID,
                    err :new Error("unknown message type"),
                    code :-1
                };
                doPost(errRes);
            }
        };
        const handlingWithTimer = (job :JobRequest) => {
            console.time(`msg handling ${job.jobID}`)
            handleMessage(job);
            console.timeEnd(`msg handling ${job.jobID}`)
        };
        setInterval(()=>{
            while(msgQueue.length > 0){
                console.timeEnd(`msg queueing ${msgQueue[0].jobID}`);
                handlingWithTimer(msgQueue.shift() as JobRequest);
            }
        },100)
    };
    const mod =await Module({
        print: (text) => {
            textCapture.stdout += text;
        },
        printErr: (text) => {
            textCapture.stderr += text;
        },
    });
    console.log("src2json worker initialized");
    onInitalized(mod);
})();
