import type { JobResult, TraceID} from "./msg.js";
import { newTraceID, traceIDCancel, traceIDGetID } from "./msg.js";

export class UpdateTracer {
    #traceID: number = 0;
    #previousTraceID: TraceID|null = null;
    getTraceID(logger: (...args: any[]) => void = console.log): TraceID {
        if(this.#previousTraceID) {
            traceIDCancel(this.#previousTraceID);
            this.#previousTraceID = null;
        }
        const new_id  = newTraceID(++this.#traceID, logger);
        this.#previousTraceID = new_id;
        return new_id;
    }
    editorAlreadyUpdated(s: JobResult, logger: (...args: any[]) => void = console.log) :boolean {
        if (this.#traceID !== traceIDGetID(s.traceID)) {
            logger(`already updated traceID: ${traceIDGetID(s.traceID)} jobID: ${s.jobID}`);
            return true;
        }
        return false;
    }
};
