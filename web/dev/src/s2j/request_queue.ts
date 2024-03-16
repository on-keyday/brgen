
import { JobRequest, JobResult } from "./msg"

export class RequestQueue {
    readonly #msgQueue: JobRequest[] = [];
    handler: ()=>void = ()=>{};

    constructor(requestHandler: ()=>void) {
        globalThis.onmessage = this.#onmessage.bind(this);
        this.handler = requestHandler;
    }

    #onmessage(e :MessageEvent) {
        this.#postRequest(e.data as JobRequest);
        this.handler();
    }

    #postRequest(ev: JobRequest) {
        console.time(`msg queueing ${ev.traceID}.${ev.jobID}`)
        this.#msgQueue.push(ev);
    }

    repostRequest(ev: JobRequest) {
        console.timeEnd(`msg handling ${ev.traceID}.${ev.jobID}`) // cancel previous
        console.log(`requeueing ${ev.traceID}.${ev.jobID}`)
        console.time(`msg queueing ${ev.traceID}.${ev.jobID}`)
        this.#msgQueue.push(ev);
    }

    popRequest(): JobRequest | undefined {
        const r = this.#msgQueue.shift();
        if(r !== undefined){
            console.timeEnd(`msg queueing ${r.traceID}.${r.jobID}`)
            console.time(`msg handling ${r.traceID}.${r.jobID}`)
        }
        return r;
    }

    postResult(msg: JobResult) {
        console.timeEnd(`msg handling ${msg.traceID}.${msg.jobID}`)
        console.time(`msg posting ${msg.traceID}.${msg.jobID}`)
        //this.#postQueue.push(msg);
        globalThis.postMessage(msg);
        console.timeEnd(`msg posting ${msg.traceID}.${msg.jobID}`)
    }
}