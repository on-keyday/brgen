
import {RequestMessage,JobRequest,JobResult }  from "./msg.js";


export class JobManager {
    #worker :Worker;
    #resolverMap = new Map<number, {resolve : (value :JobResult) => void, reject : (reason :any) => void}>();
    #jobID = 0;
    constructor(worker :Worker){
        this.#worker = worker;
        this.#worker.onmessage = this.#onmessage.bind(this);
        this.#worker.onerror = this.#onerror.bind(this);
    }

    #onerror(e :ErrorEvent) {
        console.error(e);
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

    getRequest(msg :RequestMessage,sourceCode :string) :JobRequest {
        const id = this.#jobID;
        this.#jobID++;
        const req :JobRequest = {
            msg,
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