
import { JobRequest, JobResult, TraceID, traceIDCanceled, traceIDGetID } from "./msg.js";
import { RequestQueue } from "./request_queue.js";
import { MyEmscriptenModule } from "./emscripten_mod.js";

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

    constructor(mod :EmscriptenModuleFactory<MyEmscriptenModule>,
        makeArgs: (e: JobRequest,m :MyEmscriptenModule) => string[]|Error|Promise<string[]|Error>,onload? :() => void) {
        this.#msgQueue = new RequestQueue(()=>{
            this.#handleRequest(makeArgs);
        });
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

    async #callEmscriptenMain(traceID :TraceID, args: string[]): Promise<number> {
        await this.#waitForPromise();
        let arg: string = "";
        for (let i = 0; i < args.length; i++) {
            if (i !== 0) arg += " ";
            arg += encodeURIComponent(args[i]);
        }
        try {
            if(traceIDCanceled(traceID)) {
                console.log(`canceled traceID before emscripten_main: ${traceIDGetID(traceID)} `);
                return -1;
            }
            if(this.#mod!.set_cancel_callback) {
                this.#mod!.set_cancel_callback(() => {
                    return traceIDCanceled(traceID);
                });
            }
            return this.#mod!.ccall("emscripten_main", "number", ["string"], [arg]);
        }catch(e) {
            console.log(e);
            if(e instanceof WebAssembly.RuntimeError || typeof e == "number" /*in production, this is caused because of C++ exception or other crashes */) {
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
        const code = await this.#callEmscriptenMain(e.traceID, args);
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

    async #handleRequest(makeArgs: (e: JobRequest,m :MyEmscriptenModule) => string[]|Error|Promise<string[]|Error>) {
        while(true){
            const p = this.#popRequest();
            if(p === undefined) break;
            try {
                await this.#waitForPromise();
                const args = await makeArgs(p,this.#mod!);
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
            } catch(e :any) {
                const res: JobResult = {
                    lang: p.lang,
                    jobID: p.jobID,
                    traceID: p.traceID,
                    err: e,
                    code: -1,
                }
                this.#postResult(res);
            }   
        }
    }

}

