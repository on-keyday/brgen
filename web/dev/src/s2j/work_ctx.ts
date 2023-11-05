/// <reference types="emscripten" />
export interface MyEmscriptenModule extends EmscriptenModule {
    ccall: typeof ccall;
    FS :typeof FS
}

import { JobRequest, JobResult } from "./msg.js";

export class WorkContext  {
    readonly #msgQueue: JobRequest[] = [];
    readonly #postQueue: JobResult[] = [];
    readonly #textCapture = {
        jobId: -1,
        stdout: "",
        stderr: "",
    };
    #promise :Promise<MyEmscriptenModule> ;
    #mod? :MyEmscriptenModule = undefined;

    constructor(mod :EmscriptenModuleFactory<MyEmscriptenModule>,onload? :() => void) {
        this.#msgQueue = [];
        this.#postQueue = [];
        this.#textCapture = {
            jobId: -1,
            stdout: "",
            stderr: "",
        };
        this.#promise =  mod({
            print: (text) => {
                if (this.#textCapture.jobId !== -1) {
                    this.#textCapture.stdout += text;
                }
            },
            printErr: (text) => {
                if (this.#textCapture.jobId !== -1) {
                    this.#textCapture.stderr += text;
                }
            },
        }).then((m) => {
            onload?.();
            return m;
        });
    }

    

    postRequest(ev: JobRequest) {
        console.time(`msg queueing ${ev.jobID}`)
        this.#msgQueue.push(ev);
    }

    #popRequest(): JobRequest | undefined {
        const r = this.#msgQueue.shift();
        if(r !== undefined){
            console.timeEnd(`msg queueing ${r.jobID}`)
            console.time(`msg handling ${r.jobID}`)
        }
        return r;
    }

    #postResult(msg: JobResult) {
        console.timeEnd(`msg handling ${msg.jobID}`)
        console.time(`msg posting ${msg.jobID}`)
        this.#postQueue.push(msg);
    }

    #popResult(): JobResult | undefined {
        const r = this.#postQueue.shift();
        if(r !== undefined){
            console.timeEnd(`msg posting ${r.jobID}`)
        }
        return r;
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

    async waitForPromise() {
        if(!this.#mod){
            this.#mod = await this.#promise;
        }
    }

    async callEmscriptenMain(args: string[]): Promise<number> {
        await this.waitForPromise();
        let arg: string = "";
        for (let i = 0; i < args.length; i++) {
            if (i !== 0) arg += " ";
            arg += encodeURIComponent(args[i]);
        }
        return this.#mod!.ccall("emscripten_main", "number", ["string"], [arg]);
    }

    async #exec(e :JobRequest, args :string[]) {
        const id = e.jobID;
        if (this.#textCapture.jobId !== -1) {
            const res: JobResult = {
                msg: e.msg,
                jobID: id,
                err: new Error("previous job is not finished"),
                code: -1,
            };
            this.#postResult(res);
            return;
        }
        e.options?.forEach((v) => {
            args.push(v);
        });
        this.#setCapture(id);
        const code = await this.callEmscriptenMain(args);
        const result: JobResult = {
            msg: e.msg,
            stdout: this.#textCapture.stdout,
            stderr: this.#textCapture.stderr,
            code,
            jobID: id,
        };
        this.#clearCapture();
        this.#postResult(result);
    }

    async handleRequest(makeArgs: (e: JobRequest,m :MyEmscriptenModule) => string[]|Error) {
        while(true){
            const p = this.#popRequest();
            if(p === undefined) break;
            await this.waitForPromise();
            const args = makeArgs(p,this.#mod!);
            if(args instanceof Error) {
                const res: JobResult = {
                    msg: p.msg,
                    jobID: p.jobID,
                    err: args,
                    code: -1,
                }
                this.#postResult(res);
                continue;
            }
            await this.#exec(p,args);
        }
    }

    handleResponse() {
        while(true){
            const p = this.#popResult();
            if(p === undefined) break;
            globalThis.postMessage(p);
        }
    }
}