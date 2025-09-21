import { JobResult } from "./msg.js";

export class UpdateTracer {
    #traceID: number = 0;
    getTraceID() {
        return ++this.#traceID;
    }
    editorAlreadyUpdated(s: JobResult) {
        if (this.#traceID !== s.traceID) {
            console.log(`already updated traceID: ${s.traceID} jobID: ${s.jobID}`);
            return true;
        }
        return false;
    }
};
