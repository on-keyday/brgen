import {RequestLanguage }  from "./msg.js";
import {JobManager,TraceID} from "./job_mgr.js";


const WorkerFactory = class {
    #s2j_mgr_ :JobManager |null = null;
    #j2cp_mgr_ :JobManager |null = null;
    #j2cp2_mgr_ :JobManager |null = null;
    #j2go_mgr_ :JobManager |null = null;
    #j2c_mgr_ :JobManager |null = null;
    #j2rs_mgr_ :JobManager |null = null;

    getSrc2JSONWorker = () => {
        if(this.#s2j_mgr_) return this.#s2j_mgr_;
        this.#s2j_mgr_ = new JobManager(new Worker(new URL("./src2json_worker.js",import.meta.url),{type:"module"}));
        return this.#s2j_mgr_;
    }

    getJSON2CppWorker = () => {
        if(this.#j2cp_mgr_) return this.#j2cp_mgr_;
        this.#j2cp_mgr_ = new JobManager(new Worker(new URL("./json2cpp_worker.js",import.meta.url),{type:"module"}));
        return this.#j2cp_mgr_;
    }

    getJSON2Cpp2Worker = () => {
        if(this.#j2cp2_mgr_) return this.#j2cp2_mgr_;
        this.#j2cp2_mgr_ = new JobManager(new Worker(new URL("./json2cpp2_worker.js",import.meta.url),{type:"module"}));
        return this.#j2cp2_mgr_;
    }

    getJSON2GoWorker = () => {
        if(this.#j2go_mgr_) return this.#j2go_mgr_;
        this.#j2go_mgr_ = new JobManager(new Worker(new URL("./json2go_worker.js",import.meta.url),{type:"module"}));
        return this.#j2go_mgr_;
    }

    getJSON2CWWorker = () => {
        if(this.#j2c_mgr_) return this.#j2c_mgr_;
        this.#j2c_mgr_ = new JobManager(new Worker(new URL("./json2c_worker.js",import.meta.url),{type:"module"}));
        return this.#j2c_mgr_;
    }

    getJSON2RSWorker = () => {
        if(this.#j2rs_mgr_) return this.#j2rs_mgr_;
        this.#j2rs_mgr_ = new JobManager(new Worker(new URL("./json2rust_worker.js",import.meta.url),{type:"module"}));
        return this.#j2rs_mgr_;
    }
}

const factory = new WorkerFactory();


export interface CallOption {
    filename? :string
    interpret_as_utf16? :boolean
}

export interface AstOption extends CallOption {
}

export interface CppOption extends CallOption {
    use_line_map? :boolean
    use_error? :boolean
    use_raw_union? :boolean
    use_overflow_check? :boolean
}

export interface GoOption extends CallOption {
    use_put? :boolean
}

export interface COption extends CallOption {}

export const loadWorkers = () => {
    factory.getSrc2JSONWorker();
    factory.getJSON2CppWorker();
    factory.getJSON2Cpp2Worker();
    factory.getJSON2GoWorker();
    factory.getJSON2CWWorker();
}



export const getAST = (id :TraceID,sourceCode :string,options? :AstOption) => {
    const mgr = factory.getSrc2JSONWorker();
    const req = mgr.getRequest(id,RequestLanguage.JSON_AST,sourceCode);
    if(options){
        req.arguments = [];
        if(options.filename){
            req.arguments.push( "--stdin-name",options.filename);
        }
        if(options.interpret_as_utf16) {
            req.arguments.push("--interpret-mode","utf16");
        }
    }
    return mgr.doRequest(req);
}

export const getDebugAST = (id :TraceID,sourceCode :string,options? :CallOption) => {
    const mgr = factory.getSrc2JSONWorker();
    const req = mgr.getRequest(id,RequestLanguage.JSON_DEBUG_AST,sourceCode);
    if(options){
        if(options.filename){
            req.arguments = ["--stdin-name",options.filename];
        }
    }
    return mgr.doRequest(req);
}

export const getTokens = (id :TraceID,sourceCode :string,options? :CallOption) => {
    const mgr = factory.getSrc2JSONWorker();
    const req = mgr.getRequest(id,RequestLanguage.TOKENIZE,sourceCode);
    if(options){
        req.arguments = [];
        if(options.filename){
            req.arguments.push("--stdin-name",options.filename);
        }
        if(options.interpret_as_utf16) {
            req.arguments.push("--interpret-mode","utf16");
        }
    }
    return mgr.doRequest(req);
}

export const getCppPrototypeCode = (id :TraceID,sourceCode :string,options? :CallOption) => {
    const mgr = factory.getJSON2CppWorker();
    const req= mgr.getRequest(id,RequestLanguage.CPP_PROTOTYPE,sourceCode);
    return mgr.doRequest(req);
}

export const getCppCode = (id :TraceID,sourceCode :string,options? :CppOption) => {
    const mgr = factory.getJSON2Cpp2Worker();
    const req = mgr.getRequest(id,RequestLanguage.CPP,sourceCode);
    req.arguments = [];
    if(options?.use_line_map){
        req.arguments.push("--add-line-map");
    }
    if(options?.use_error){
        req.arguments.push("--use-error");
    }
    if(options?.use_raw_union){
        req.arguments.push("--use-raw-union");
    }
    if(options?.use_overflow_check) {
        req.arguments.push("--use-overflow-check");
    }
    return mgr.doRequest(req);
}

export const getGoCode = (id :TraceID,sourceCode :string,options? :GoOption) => {
    const mgr = factory.getJSON2GoWorker();
    const req = mgr.getRequest(id,RequestLanguage.GO,sourceCode);
    if(options?.use_put){
        req.arguments = ["-use-put"];
    }
    return mgr.doRequest(req);
}

export const getCCode = (id :TraceID,sourceCode :string,options? :COption) => {
    const mgr = factory.getJSON2CWWorker();
    const req = mgr.getRequest(id,RequestLanguage.C,sourceCode);
    return mgr.doRequest(req);
}
