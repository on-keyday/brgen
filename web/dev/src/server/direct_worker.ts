import { IWorker } from "../s2j/job_mgr.js";
import { JobRequest, JobResult, traceIDCanceled } from "../s2j/msg.js";
import { MyEmscriptenModule } from "../s2j/emscripten_mod.js";

/**
 * Handler function that processes a JobRequest and returns a JobResult.
 * Used by DirectWorker to execute WASM without Web Workers.
 */
export type WorkerHandler = (msg: JobRequest) => Promise<JobResult>;

/**
 * DirectWorker implements IWorker by directly invoking a handler function
 * instead of delegating to a Web Worker thread.
 *
 * It emulates the postMessage/onmessage pattern so that JobManager
 * can use it identically to a real Web Worker.
 */
export class DirectWorker implements IWorker {
    onmessage: ((msg: MessageEvent) => void) | null = null;
    onerror: ((msg: ErrorEvent) => void) | null = null;
    #handler: WorkerHandler;

    constructor(handler: WorkerHandler) {
        this.#handler = handler;
    }

    postMessage(msg: JobRequest): void {
        this.#handler(msg).then(
            (result) => {
                if (this.onmessage) {
                    this.onmessage({ data: result } as MessageEvent);
                }
            },
            (error) => {
                const errorResult: JobResult = {
                    lang: msg.lang,
                    jobID: msg.jobID,
                    traceID: msg.traceID,
                    code: -1,
                    err: error,
                };
                if (this.onmessage) {
                    this.onmessage({ data: errorResult } as MessageEvent);
                }
            }
        );
    }
}

/**
 * Creates a WorkerHandler for Emscripten-based WASM modules.
 * Replicates the logic of EmWorkContext but runs directly in the main thread.
 *
 * Accepts either a factory directly or a Promise<factory> (for dynamic imports).
 * Module loading starts eagerly when the handler is created.
 *
 * @param factoryOrPromise  Emscripten module factory (or a promise resolving to one)
 * @param makeArgs          Callback that builds command-line arguments from a JobRequest
 */
export function createEmscriptenHandler(
    factoryOrPromise: EmscriptenModuleFactory<MyEmscriptenModule> | Promise<EmscriptenModuleFactory<MyEmscriptenModule>>,
    makeArgs: (e: JobRequest, m: MyEmscriptenModule) => string[] | Error | Promise<string[] | Error>,
): WorkerHandler {
    let mod: MyEmscriptenModule | undefined;
    let modPromise: Promise<MyEmscriptenModule>;
    let resolvedFactory: EmscriptenModuleFactory<MyEmscriptenModule> | undefined;
    const textCapture = { jobId: -1, stdout: "", stderr: "" };

    function createModule(factory: EmscriptenModuleFactory<MyEmscriptenModule>): Promise<MyEmscriptenModule> {
        resolvedFactory = factory;
        mod = undefined;
        return factory({
            print: (text: string) => {
                if (textCapture.jobId !== -1) {
                    textCapture.stdout += text + "\n";
                }
            },
            printErr: (text: string) => {
                if (textCapture.jobId !== -1) {
                    textCapture.stderr += text + "\n";
                }
            },
        }).then((m) => {
            mod = m;
            return m;
        });
    }

    // Start loading eagerly
    modPromise = Promise.resolve(factoryOrPromise).then((factory) => createModule(factory));

    async function waitForModule(): Promise<MyEmscriptenModule> {
        if (mod) return mod;
        return modPromise;
    }

    function reloadModule() {
        if (resolvedFactory) {
            modPromise = createModule(resolvedFactory);
        }
    }

    async function callEmscriptenMain(traceID: JobRequest["traceID"], args: string[]): Promise<number> {
        const m = await waitForModule();
        let arg = "";
        for (let i = 0; i < args.length; i++) {
            if (i !== 0) arg += " ";
            arg += encodeURIComponent(args[i]);
        }
        try {
            if (traceIDCanceled(traceID)) {
                return -1;
            }
            if (m.set_cancel_callback) {
                m.set_cancel_callback(() => traceIDCanceled(traceID));
            }
            return m.ccall("emscripten_main", "number", ["string"], [arg]);
        } catch (e) {
            if (e instanceof WebAssembly.RuntimeError || typeof e === "number") {
                reloadModule();
            }
            return -1;
        }
    }

    return async (req: JobRequest): Promise<JobResult> => {
        const m = await waitForModule();
        const args = await makeArgs(req, m);
        if (args instanceof Error) {
            return {
                lang: req.lang,
                jobID: req.jobID,
                traceID: req.traceID,
                err: args,
                code: -1,
            };
        }

        if (req.arguments) {
            args.push(...req.arguments);
        }

        textCapture.jobId = req.jobID;
        textCapture.stdout = "";
        textCapture.stderr = "";

        const code = await callEmscriptenMain(req.traceID, args);

        const result: JobResult = {
            lang: req.lang,
            stdout: textCapture.stdout,
            stderr: textCapture.stderr,
            originalSourceCode: req.sourceCode,
            code,
            jobID: req.jobID,
            traceID: req.traceID,
        };

        textCapture.jobId = -1;
        textCapture.stdout = "";
        textCapture.stderr = "";

        return result;
    };
}

/**
 * Creates a WorkerHandler for Go-based WASM modules.
 * Replicates the logic of GoWorkContext but runs directly in the main thread.
 *
 * @param wasmPath  Path to the .wasm file (resolved via node:fs)
 * @param makeArgs  Callback that extracts source code from a JobRequest
 * @param toolName  Name of the Go tool (e.g., "json2go", "json2kaitai") used as argv[0]
 */
export function createGoHandler(
    wasmPath: string,
    makeArgs: (e: JobRequest) => string | Error,
    toolName: string,
): WorkerHandler {
    let go: any;
    let wasmModule: WebAssembly.Module | undefined;
    let instance: WebAssembly.Instance | undefined;

    async function loadGoRuntime() {
        await import("../lib/go_wasm_exec.js");
        go = new (globalThis as any).Go();
    }

    async function waitForLoad() {
        if (!go) {
            await loadGoRuntime();
        }
        if (!wasmModule) {
            const fs = await import("node:fs/promises");
            const data = await fs.readFile(wasmPath);
            wasmModule = await WebAssembly.compile(data);
        }
        if (!instance) {
            instance = await WebAssembly.instantiate(wasmModule, go.importObject);
            const reload = () => {
                (globalThis as any).Go.prototype.json2goGenerator = undefined;
                go = new (globalThis as any).Go();
                instance = undefined;
            };
            go.run(instance).then(reload, reload);
            if (go.json2goGenerator === undefined) {
                throw new Error(`failed to load json2goGenerator via go.run for ${toolName}`);
            }
        }
    }

    return async (req: JobRequest): Promise<JobResult> => {
        const src = makeArgs(req);
        if (src instanceof Error) {
            return {
                lang: req.lang,
                jobID: req.jobID,
                traceID: req.traceID,
                err: src,
                code: -1,
            };
        }

        await waitForLoad();

        const args = [toolName];
        if (req.arguments) {
            args.push(...req.arguments);
        }

        try {
            const result = go.json2goGenerator!(src, args);
            return {
                lang: req.lang,
                jobID: req.jobID,
                traceID: req.traceID,
                code: result.code,
                stdout: result.stdout,
                stderr: result.stderr,
            };
        } catch (e: any) {
            return {
                lang: req.lang,
                jobID: req.jobID,
                traceID: req.traceID,
                err: e,
                code: -1,
            };
        }
    };
}

/**
 * Creates a WorkerHandler for the json2rust wasm-pack module.
 * Directly calls the json2rust() function without Emscripten or Go runtimes.
 */
export function createRustWasmHandler(): WorkerHandler {
    let initialized = false;

    return async (req: JobRequest): Promise<JobResult> => {
        const { default: _wbg_init, json2rust } = await import("json2rust");
        if (!initialized) {
            await _wbg_init();
            initialized = true;
        }
        try {
            const result = json2rust(req.sourceCode);
            return {
                lang: req.lang,
                jobID: req.jobID,
                traceID: req.traceID,
                originalSourceCode: req.sourceCode,
                code: 0,
                stdout: result,
                stderr: "",
            };
        } catch (e: any) {
            return {
                lang: req.lang,
                jobID: req.jobID,
                traceID: req.traceID,
                originalSourceCode: req.sourceCode,
                code: 1,
                stdout: "",
                stderr: e.toString(),
            };
        }
    };
}
