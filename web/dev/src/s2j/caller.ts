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


export interface CallOption {
    filename? :string
}

export interface AstOption extends CallOption {
    interpret_as_utf16? :boolean
}

export interface CppOption extends CallOption {
    use_line_map? :boolean
}

export interface GoOption extends CallOption {
    use_put? :boolean
}

export const loadWorkers = () => {
    factory.getSrc2JSONWorker();
    factory.getJSON2CppWorker();
    factory.getJSON2Cpp2Worker();
    factory.getJSON2GoWorker();
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

export const getAST = (sourceCode :string,options? :AstOption) => {
    const mgr = factory.getSrc2JSONWorker();
    const req = mgr.getRequest(RequestLanguage.JSON_AST,sourceCode);
    if(options){
        if(options.filename){
            req.arguments = ["--stdin-name",options.filename];
        }
        if(options.interpret_as_utf16) {
            req.arguments = ["--interpret-mode","utf16"]
        }
    }
    return mgr.doRequest(req);
}

export const getDebugAST = (sourceCode :string,options? :CallOption) => {
    const mgr = factory.getSrc2JSONWorker();
    const req = mgr.getRequest(RequestLanguage.JSON_DEBUG_AST,sourceCode);
    if(options){
        if(options.filename){
            req.arguments = ["--stdin-name",options.filename];
        }
    }
    return mgr.doRequest(req);
}

export const getTokens = (sourceCode :string,options? :CallOption) => {
    const mgr = factory.getSrc2JSONWorker();
    const req = mgr.getRequest(RequestLanguage.TOKENIZE,sourceCode);
    if(options){
        if(options.filename){
            req.arguments = ["--stdin-name",options.filename];
        }
    }
    return mgr.doRequest(req);
}

export const getCppPrototypeCode = (sourceCode :string,options? :CallOption) => {
    const mgr = factory.getJSON2CppWorker();
    const req= mgr.getRequest(RequestLanguage.CPP_PROTOTYPE,sourceCode);
    return mgr.doRequest(req);
}

export const getCppCode = (sourceCode :string,options? :CppOption) => {
    const mgr = factory.getJSON2Cpp2Worker();
    const req = mgr.getRequest(RequestLanguage.CPP,sourceCode);
    if(options?.use_line_map){
        req.arguments = ["--add-line-map"];
    }
    return mgr.doRequest(req);
}

export const getGoCode = (sourceCode :string,options? :GoOption) => {
    const mgr = factory.getJSON2GoWorker();
    const req = mgr.getRequest(RequestLanguage.GO,sourceCode);
    if(options?.use_put){
        req.arguments = ["-use-put"];
    }
    return mgr.doRequest(req);
}