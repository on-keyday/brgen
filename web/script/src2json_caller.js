import { RequestMessage } from "./msg.js";
const work = new Worker("./script/invoke_src2json.js", { type: "module" });
let jobID = 0;
var resolverMap = new Map();
work.onmessage = (e) => {
    const msg = e.data;
    const resolver = resolverMap.get(msg.jobID);
    if (resolver) {
        if (msg.code === 0) {
            resolver.resolve(msg);
        }
        else {
            resolver.reject(msg);
        }
        resolverMap.delete(msg.jobID);
    }
};
work.onerror = (e) => {
    console.error(e);
};
export const runSrc2JSON = (sourceCode) => {
    return new Promise((resolve, reject) => {
        const id = jobID;
        jobID++;
        const req = {
            msg: RequestMessage.MSG_SOURCE_CODE,
            jobID: id,
            sourceCode
        };
        resolverMap.set(id, { resolve, reject });
        work.postMessage(req);
    });
};
//# sourceMappingURL=src2json_caller.js.map