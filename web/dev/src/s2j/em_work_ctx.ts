/// <reference types="emscripten" />
export interface MyEmscriptenModule extends EmscriptenModule {
    ccall: typeof ccall;
    FS :typeof FS
}

import { JobRequest, JobResult } from "./msg.js";
import { RequestQueue } from "./request_queue.js";



export class EmWorkContext  {
    readonly #msgQueue: RequestQueue;
    readonly #textCapture = {
        jobId: -1,
        stdout: "",
        stderr: "",
    };
    #promise :Promise<MyEmscriptenModule> | undefined;
    #mod? :MyEmscriptenModule = undefined;
    #factory :EmscriptenModuleFactory<MyEmscriptenModule>;
    #onload? :() => void;

    constructor(mod :EmscriptenModuleFactory<MyEmscriptenModule>,onload? :() => void) {
        this.#msgQueue = new RequestQueue();
        this.#textCapture = {
            jobId: -1,
            stdout: "",
            stderr: "",
        };
        this.#factory = mod;
        this.#onload = onload;
        this.#initModule();
    }

    #initModule() { 
        this.#mod = undefined;
        this.#promise = this.#factory({
            print: (text) => {
                if (this.#textCapture.jobId !== -1) {
                    this.#textCapture.stdout += text+"\n";
                }
            },
            printErr: (text) => {
                if (this.#textCapture.jobId !== -1) {
                    this.#textCapture.stderr += text+"\n";
                }
            },
        }).then((m) => {
            if(this.#onload !== undefined) this.#onload();
            return m;
        });
    }

    

    postRequest(ev: JobRequest) {
        this.#msgQueue.postRequest(ev);
    }

    #popRequest(): JobRequest | undefined {
        return this.#msgQueue.popRequest();
    }

    #postResult(msg: JobResult) {
        this.#msgQueue.postResult(msg);
    }


    #clearCapture() {
        this.#textCapture.jobId = -1;
        this.#textCapture.stdout = "";
        this.#textCapture.stderr = "";
    }

    #setCapture(jobId: number) {
        this.#textCapture.jobId = jobId;
        this.#textCapture.stdout = "";
        this.#textCapture.stderr = "";
    }

    async #waitForPromise() {
        if(!this.#mod){
            this.#mod = await this.#promise;
        }
    }

    async callEmscriptenMain(args: string[]): Promise<number> {
        await this.#waitForPromise();
        let arg: string = "";
        for (let i = 0; i < args.length; i++) {
            if (i !== 0) arg += " ";
            arg += encodeURIComponent(args[i]);
        }
        try {
            return this.#mod!.ccall("emscripten_main", "number", ["string"], [arg]);
        }catch(e) {
            console.log(e);
            if(e instanceof WebAssembly.RuntimeError) {
                console.log("reloading webassembly module")
                this.#initModule();// reload module
            }
            return -1;
        }
    }

    async #exec(e :JobRequest, args :string[]) {
        const id = e.jobID;
        if (this.#textCapture.jobId !== -1) {
            this.#msgQueue.repostRequest(e);
            return;
        }
        e.arguments?.forEach((v) => {
            args.push(v);
        });
        this.#setCapture(id);
        const code = await this.callEmscriptenMain(args);
        const result: JobResult = {
            lang: e.lang,
            stdout: this.#textCapture.stdout,
            stderr: this.#textCapture.stderr,
            originalSourceCode: e.sourceCode,
            code,
            jobID: id,
            traceID: e.traceID,
        };
        this.#clearCapture();
        this.#postResult(result);
    }

    async handleRequest(makeArgs: (e: JobRequest,m :MyEmscriptenModule) => string[]|Error) {
        while(true){
            const p = this.#popRequest();
            if(p === undefined) break;
            await this.#waitForPromise();
            const args = makeArgs(p,this.#mod!);
            if(args instanceof Error) {
                const res: JobResult = {
                    lang: p.lang,
                    jobID: p.jobID,
                    traceID: p.traceID,
                    err: args,
                    code: -1,
                }
                this.#postResult(res);
                continue;
            }
            await this.#exec(p,args);
        }
    }


}

