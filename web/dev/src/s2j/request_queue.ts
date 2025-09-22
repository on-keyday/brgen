
import { JobRequest, JobResult, traceIDGetID } from "./msg.js"

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
        console.time(`msg queueing ${traceIDGetID(ev.traceID)}.${ev.jobID}`)
        this.#msgQueue.push(ev);
    }

    repostRequest(ev: JobRequest) {
        console.timeEnd(`msg handling ${traceIDGetID(ev.traceID)}.${ev.jobID}`) // cancel previous
        console.log(`requeueing ${traceIDGetID(ev.traceID)}.${ev.jobID}`)
        console.time(`msg queueing ${traceIDGetID(ev.traceID)}.${ev.jobID}`)
        this.#msgQueue.push(ev);
    }

    popRequest(): JobRequest | undefined {
        const r = this.#msgQueue.shift();
        if(r !== undefined){
            console.timeEnd(`msg queueing ${traceIDGetID(r.traceID)}.${r.jobID}`)
            console.time(`msg handling ${traceIDGetID(r.traceID)}.${r.jobID}`)
        }
        return r;
    }

    postResult(msg: JobResult) {
        console.timeEnd(`msg handling ${traceIDGetID(msg.traceID)}.${msg.jobID}`)
        console.time(`msg posting ${traceIDGetID(msg.traceID)}.${msg.jobID}`)
        //this.#postQueue.push(msg);
        globalThis.postMessage(msg);
        console.timeEnd(`msg posting ${traceIDGetID(msg.traceID)}.${msg.jobID}`)
    }
}