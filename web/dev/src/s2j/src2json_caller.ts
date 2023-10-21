import {RequestMessage,JobRequest,JobResult }  from "./msg.js";
const work = new Worker(new URL("./src2json_worker.js",import.meta.url),{type:"module"});

let jobID = 0;

var resolverMap = new Map<number, {resolve : (value :JobResult) => void, reject : (reason :any) => void}>();

work.onmessage = (e) => {
    const msg = e.data as JobResult;
    const resolver = resolverMap.get(msg.jobID);
    if(resolver){
        resolverMap.delete(msg.jobID);
        if(msg.code===0){
            resolver.resolve(msg);
        }else{
            resolver.reject(msg);
        }
    }
};

work.onerror = (e) => {
    console.error(e);
}

export const getAST = (sourceCode :string) => {
    return new Promise<JobResult>((resolve,reject) => {
        const id = jobID;
        jobID++;
        const req :JobRequest = {
            msg :RequestMessage.MSG_REQUIRE_AST,
            jobID :id,
            sourceCode
        };
        resolverMap.set(id,{resolve,reject});
        work.postMessage(req);
    });
}

export const getTokens = (sourceCode :string) => {
    return new Promise<JobResult>((resolve,reject) => {
        const id = jobID;
        jobID++;
        const req :JobRequest = {
            msg :RequestMessage.MSG_REQUIRE_TOKENS,
            jobID :id,
            sourceCode
        };
        resolverMap.set(id,{resolve,reject});
        work.postMessage(req);
    });
}

