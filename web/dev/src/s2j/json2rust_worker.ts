
import _wbg_init, { InitOutput, json2rust } from "json2rust"
import { JobRequest, JobResult } from "./msg";

let _mod :InitOutput | null = null;

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
        const result = json2rust(data.sourceCode!);
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
