/// <reference types="emscripten" />
export interface MyEmscriptenModule extends EmscriptenModule {
    ccall: typeof ccall;
    FS :typeof FS
}

import { JobRequest, JobResult } from "./msg.js";
import "../lib/go_wasm_exec";

declare class Go {
    constructor();
    run(instance: WebAssembly.Instance): Promise<void>;
    importObject: WebAssembly.Imports;
    // for use in the browser set by go
    json2goGenerator :((sourceCode :string,args :string[]) => { stdout: string , stderr: string , code: number}) | undefined;
}


class RequestQueue {
    readonly #msgQueue: JobRequest[] = [];
    //readonly #postQueue: JobResult[] = [];

    constructor() {}

    postRequest(ev: JobRequest) {
        console.time(`msg queueing ${ev.jobID}`)
        this.#msgQueue.push(ev);
    }

    repostRequest(ev: JobRequest) {
        console.timeEnd(`msg handling ${ev.jobID}`) // cancel previous
        console.log(`requeueing ${ev.jobID}`)
        console.time(`msg queueing ${ev.jobID}`)
        this.#msgQueue.push(ev);
    }

    popRequest(): JobRequest | undefined {
        const r = this.#msgQueue.shift();
        if(r !== undefined){
            console.timeEnd(`msg queueing ${r.jobID}`)
            console.time(`msg handling ${r.jobID}`)
        }
        return r;
    }

    postResult(msg: JobResult) {
        console.timeEnd(`msg handling ${msg.jobID}`)
        console.time(`msg posting ${msg.jobID}`)
        //this.#postQueue.push(msg);
        globalThis.postMessage(msg);
        console.timeEnd(`msg posting ${msg.jobID}`)
    }

    /*
    #popResult(): JobResult | undefined {
        const r = this.#postQueue.shift();
        if(r !== undefined){
            console.timeEnd(`msg posting ${r.jobID}`)
        }
        return r;
    }

    handleResponse() {
        while(true){
            const p = this.#popResult();
            if(p === undefined) break;
            globalThis.postMessage(p);
        }
    }
    */
}

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
                    err: args,
                    code: -1,
                }
                this.#postResult(res);
                continue;
            }
            await this.#exec(p,args);
        }
    }

    /*
    handleResponse() {
        this.#msgQueue.handleResponse();
    }
    */

}

export class GoWorkContext  {
    #go :Go;
    #src :Promise<BufferSource> | undefined;
    #wasm :WebAssembly.Module | undefined;
    #instance :WebAssembly.Instance | undefined;
    #msgQueue: RequestQueue;
    #onload? :() => void;

    constructor(p :Promise<BufferSource> ,onload? :() => void) {
        this.#msgQueue = new RequestQueue();
        this.#onload = onload; 
        this.#src = p;    
        this.#go = new Go();
    }

    async #waitForLoad() {
        if(!this.#wasm) {
            const data =await WebAssembly.compile(await this.#src!);
            this.#src = undefined;
            this.#wasm = data;
        }
        if(!this.#instance) {
            this.#instance = await WebAssembly.instantiate(this.#wasm,this.#go.importObject);
            // do export json2GoGenerator
            const reload = (e :any) =>{
                console.log(e);
                console.log("reloading webassembly module")
                Go.prototype.json2goGenerator = undefined;
                this.#initModule();// reload module
            }
            this.#go.run(this.#instance).then(reload,reload);
            if(this.#go.json2goGenerator === undefined) {
                throw new Error("failed to load json2goGenerator via go.run");
            }
            if(this.#onload !== undefined) {
                this.#onload();
            }
        }
    }

    #initModule() {
        this.#go = new Go();
        this.#instance = undefined;
    }

    async #exec(e :JobRequest, srcCode :string,args :string[]) {
        await this.#waitForLoad();
        return this.#go.json2goGenerator!(srcCode,args);
    }
   
    postRequest(ev: JobRequest) {
        this.#msgQueue.postRequest(ev);
    }

    async handleRequest(makeArgs: (e: JobRequest) => string|Error) {
        while(true){
            const p = this.#msgQueue.popRequest();
            if(p === undefined) break;
            const src = makeArgs(p);
            if(src instanceof Error) {
                const res: JobResult = {
                    lang: p.lang,
                    jobID: p.jobID,
                    err: src,
                    code: -1,
                }
                this.#msgQueue.postResult(res);
                continue;
            }
            const arg = ["json2go"]
            if(p.arguments !== undefined) {
                arg.push(...p.arguments);
            }
            const result = await this.#exec(p,src,arg);
            const res: JobResult = {
                lang: p.lang,
                jobID: p.jobID,
                code: result.code,
                stdout: result.stdout,
                stderr: result.stderr,
            }
            this.#msgQueue.postResult(res);
        }
    }

    /*
    handleResponse() {
        this.#msgQueue.handleResponse();
    }
    */
}
