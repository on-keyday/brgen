import { JobResult, newTraceID, TraceID, traceIDCancel, traceIDGetID } from "./msg.js";

export class UpdateTracer {
    #traceID: number = 0;
    #previousTraceID: TraceID|null = null;
    getTraceID(): TraceID {
        if(this.#previousTraceID) {
            traceIDCancel(this.#previousTraceID);
            this.#previousTraceID = null;
        }
        const new_id  = newTraceID(++this.#traceID);
        this.#previousTraceID = new_id;
        return new_id;
    }
    editorAlreadyUpdated(s: JobResult) :boolean {
        if (this.#traceID !== traceIDGetID(s.traceID)) {
            console.log(`already updated traceID: ${traceIDGetID(s.traceID)} jobID: ${s.jobID}`);
            return true;
        }
        return false;
    }
};
