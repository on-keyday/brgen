import {RequestLanguage,JobRequest,JobResult }  from "./msg.js";
import {JobManager} from "./job_mgr.js";


const WorkerFactory = class {
    #s2j_mgr_ :JobManager |null = null;
    #j2c_mgr_ :JobManager |null = null;
    #j2c2_mgr_ :JobManager |null = null;
    #j2go_mgr_ :JobManager |null = null;

    getSrc2JSONWorker = () => {
        if(this.#s2j_mgr_) return this.#s2j_mgr_;
        this.#s2j_mgr_ = new JobManager(new Worker(new URL("./src2json_worker.js",import.meta.url),{type:"module"}));
        return this.#s2j_mgr_;
    }

    getJSON2CppWorker = () => {
        if(this.#j2c_mgr_) return this.#j2c_mgr_;
        this.#j2c_mgr_ = new JobManager(new Worker(new URL("./json2cpp_worker.js",import.meta.url),{type:"module"}));
        return this.#j2c_mgr_;
    }

    getJSON2Cpp2Worker = () => {
        if(this.#j2c2_mgr_) return this.#j2c2_mgr_;
        this.#j2c2_mgr_ = new JobManager(new Worker(new URL("./json2cpp2_worker.js",import.meta.url),{type:"module"}));
        return this.#j2c2_mgr_;
    }

    getJSON2GoWorker = () => {
        if(this.#j2go_mgr_) return this.#j2go_mgr_;
        this.#j2go_mgr_ = new JobManager(new Worker(new URL("./json2go_worker.js",import.meta.url),{type:"module"}));
        return this.#j2go_mgr_;
    }
}

const factory = new WorkerFactory();


interface CallOption {
    filename? :string
}

export const loadWorkers = () => {
    factory.getSrc2JSONWorker();
    factory.getJSON2CppWorker();
}

const getRequest = (mgr :JobManager , lang :RequestLanguage,sourceCode :string,options? :CallOption) :JobRequest => {
    const req = mgr.getRequest(lang,sourceCode);
    if(options){
        if(options.filename){
            req.arguments = ["--stdin-name",options.filename];
        }
    }
    return req;
}

export const getAST = (sourceCode :string,options? :CallOption) => {
    const mgr = factory.getSrc2JSONWorker();
    return mgr.doRequest(getRequest(mgr,RequestLanguage.JSON_AST,sourceCode,options));
}

export const getTokens = (sourceCode :string,options? :CallOption) => {
    const mgr = factory.getSrc2JSONWorker();
    return mgr.doRequest(getRequest(mgr,RequestLanguage.TOKENIZE,sourceCode,options));
}

export const getCppCode = (sourceCode :string,options? :CallOption) => {
    const mgr = factory.getJSON2CppWorker();
    return mgr.doRequest(getRequest(mgr,RequestLanguage.CPP_PROTOTYPE,sourceCode,{}));
}
