
import type { InitOutput} from "json2rust";
import _wbg_init, { json2rust } from "json2rust"
import type { JobRequest, JobResult } from "../msg.js";
import { fetchOrReadWasm } from "./fetch_or_read.js";

let _mod :InitOutput | null = null;

// intercept fetch call so that we do not need to know where the json2rust.wasm is located
const originalFetch = globalThis.fetch;
// @ts-ignore
globalThis.fetch = async (input: RequestInfo, init?: RequestInit) => {
    const respBuffer = await fetchOrReadWasm(input);
    return new Response(respBuffer);
}

const getWasmModule = async () => {
    if (_mod) return _mod;
    const mod = await _wbg_init();
    if(_mod !== null) return _mod;
    _mod = mod;
    return _mod;
}

globalThis.onmessage = async(ev) => {
    const data = ev.data as JobRequest;
    await getWasmModule();
    try {
        const result = json2rust(data.sourceCode);
        const res :JobResult = {
            lang: data.lang,
            jobID: data.jobID,
            traceID: data.traceID,
            originalSourceCode: data.sourceCode,
            code: 0,
            stdout: result,
            stderr: ""
        }
        globalThis.postMessage(res);
    } catch(e) {
        console.error(e)
        const res :JobResult = {
            lang: data.lang,
            jobID: data.jobID,
            traceID: data.traceID,
            originalSourceCode: data.sourceCode,
            code: 1,
            stdout: "",
            stderr: (e as any).toString()
        }
        globalThis.postMessage(res);
    }
}
