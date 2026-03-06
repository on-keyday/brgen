

import type {JobRequest,JobResult, RequestLanguage,TraceID }  from "./msg.js";


export interface IWorker {
    addEventListener: (type :string, listener : (this: any, ev: MessageEvent) => any) => void;
    postMessage: (msg :JobRequest) => void;
}
export class JobManager {
    #worker :IWorker;
    #resolverMap = new Map<number, {resolve : (value :JobResult) => void, reject : (reason :any) => void}>();
    #jobID = 0;
    constructor(worker :IWorker){
        this.#worker = worker;
        this.#worker.addEventListener("message", (e) => this.#onmessage(e));
        this.#worker.addEventListener("error", (e) => this.#onerror(e as any));
    }

    #onerror(e :ErrorEvent) {
        console.error("Error detected!",e);
    }

    #onmessage(e :MessageEvent) {
        const msg = e.data as JobResult;
        const resolver = this.#resolverMap.get(msg.jobID);
        if(resolver){
            this.#resolverMap.delete(msg.jobID);
            if(msg.code===0){
                resolver.resolve(msg);
            }else{
                resolver.reject(msg);
            }
        }
    }

    getRequest(traceID :TraceID,lang :RequestLanguage,sourceCode :string) :JobRequest {
        const id = this.#jobID;
        this.#jobID++;
        const req :JobRequest = {
            lang,
            traceID,
            jobID :id,
            sourceCode
        };
        return req;
    }

    doRequest(req :JobRequest) :Promise<JobResult> {
        return new Promise<JobResult>((resolve,reject) => {
            this.#resolverMap.set(req.jobID,{resolve,reject});
            this.#worker.postMessage(req);
        });
    }
}