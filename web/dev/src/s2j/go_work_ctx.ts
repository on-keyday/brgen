
import "../lib/go_wasm_exec.js";
import { RequestQueue } from "./request_queue";
import { JobRequest, JobResult } from "./msg"

declare class Go {
    constructor();
    run(instance: WebAssembly.Instance): Promise<void>;
    importObject: WebAssembly.Imports;
    // for use in the browser set by go
    json2goGenerator? :(sourceCode :string,args :string[]) => { stdout: string , stderr: string , code: number};
}


export class GoWorkContext  {
    #go :Go;
    #src :Promise<BufferSource> | undefined;
    #wasm :WebAssembly.Module | undefined;
    #instance :WebAssembly.Instance | undefined;
    #msgQueue: RequestQueue;
    #onload? :() => void;

    constructor(p :Promise<BufferSource> ,makeArgs: (e: JobRequest) => string|Error,onload? :() => void){
        this.#msgQueue = new RequestQueue(()=>{
            this.#handleRequest(makeArgs)
        });
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
       

    async #handleRequest(makeArgs: (e: JobRequest) => string|Error) {
        while(true){
            const p = this.#msgQueue.popRequest();
            if(p === undefined) break;
            const src = makeArgs(p);
            if(src instanceof Error) {
                const res: JobResult = {
                    lang: p.lang,
                    jobID: p.jobID,
                    traceID: p.traceID,
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
                traceID: p.traceID,
                code: result.code,
                stdout: result.stdout,
                stderr: result.stderr,
            }
            this.#msgQueue.postResult(res);
        }
    }

}
