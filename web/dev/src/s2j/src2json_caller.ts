import {RequestMessage,JobRequest,JobResult }  from "./msg.js";
import {JobManager} from "./job_mgr.js";

const work = new Worker(new URL("./src2json_worker.js",import.meta.url),{type:"module"});

const mgr = new JobManager(work);

interface CallOption {
    filename? :string
}

const getRequest = (msg :RequestMessage,sourceCode :string,options? :CallOption) :JobRequest => {
    const req = mgr.getRequest(msg,sourceCode);
    if(options){
        if(options.filename){
            req.options = ["--stdin-name",options.filename];
        }
    }
    return req;
}

export const getAST = (sourceCode :string,options? :CallOption) => {
    return mgr.doRequest(getRequest(RequestMessage.MSG_REQUIRE_AST,sourceCode,options));
}

export const getTokens = (sourceCode :string,options? :CallOption) => {
    return mgr.doRequest(getRequest(RequestMessage.MSG_REQUIRE_TOKENS,sourceCode,options));
}

