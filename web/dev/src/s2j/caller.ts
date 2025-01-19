import {AstOption, BMGenOption, COption, CallOption, Cpp2Option, CppOption, GoOption, JobRequest, LanguageKey, LanguageList, LanguageToOptionType, LanguageToWorkerType, RequestLanguage, Rust2Option, RustOption, TSOption, WorkerList, WorkerType }  from "./msg.js";
import {JobManager,TraceID} from "./job_mgr.js";

const workerMap = Object.freeze({
    [WorkerType.SRC2JSON]:()=> new Worker(new URL("./worker/src2json_worker.js",import.meta.url),{type:"module"}),
    [WorkerType.JSON2CPP2]:()=> new Worker(new URL("./worker/json2cpp2_worker.js",import.meta.url),{type:"module"}),
    [WorkerType.JSON2GO]:()=> new Worker(new URL("./worker/json2go_worker.js",import.meta.url),{type:"module"}),
    [WorkerType.JSON2C]:()=> new Worker(new URL("./worker/json2c_worker.js",import.meta.url),{type:"module"}),
    [WorkerType.JSON2RUST]:()=> new Worker(new URL("./worker/json2rust_worker.js",import.meta.url),{type:"module"}),
    [WorkerType.JSON2TS]:()=> new Worker(new URL("./worker/json2ts_worker.js",import.meta.url),{type:"module"}),
    [WorkerType.JSON2KAITAI]:()=> new Worker(new URL("./worker/json2kaitai_worker.js",import.meta.url),{type:"module"}),

    [WorkerType.BMGEN]:()=> new Worker(new URL("./worker/bmgen/bmgen_worker.js",import.meta.url),{type:"module"}),
    [WorkerType.BM2CPP]:()=> new Worker(new URL("./worker/bmgen/bm2cpp_worker.js",import.meta.url),{type:"module"}),
    [WorkerType.BM2RUST]:()=> new Worker(new URL("./worker/bmgen/bm2rust_worker.js",import.meta.url),{type:"module"}),
});


const WorkerFactory = class {

    #jobs = new Map<WorkerType,JobManager>();

    getWorker = (lang :WorkerType) => {
        let mgr = this.#jobs.get(lang);
        if(mgr) return mgr;
        const worker = workerMap[lang]();
        mgr = new JobManager(worker);
        this.#jobs.set(lang,mgr);
        return mgr;
    }

  
}

const factory = new WorkerFactory();

const argConverter = Object.freeze({
    [RequestLanguage.TOKENIZE] : (opt :CallOption) => {
        const args = [];
        if(opt.filename){
            args.push("--stdin-name",opt.filename);
        }
        if(opt.interpret_as_utf16) {
            args.push("--interpret-mode","utf16");
        }
        return args;
    },
    [RequestLanguage.JSON_AST] : (opt :AstOption) => {
        const args = [];
        if(opt.filename){
            args.push("--stdin-name",opt.filename);
        }
        if(opt.interpret_as_utf16) {
            args.push("--interpret-mode","utf16");
        }
        return args;
    },
    [RequestLanguage.JSON_DEBUG_AST] : (opt :CallOption) => {
        const args = [];
        if(opt.filename){
            args.push("--stdin-name",opt.filename);
        }
        return args;
    },
    [RequestLanguage.CPP] : (opt :CppOption) => {
        const args = ["--file"];
        if(opt.use_line_map){
            args.push("--add-line-map");
        }
        if(opt.use_error){
            args.push("--use-error");
        }
        if(opt.use_raw_union){
            args.push("--use-raw-union");
        }
        if(opt.use_overflow_check) {
            args.push("--use-overflow-check");
        }
        if(opt.enum_stringer){
            args.push("--enum-stringer");
        }
        if(opt.add_visit){
            args.push("--add-visit");
        }
        if(opt.use_constexpr){
            args.push("--use-constexpr");
        }
        return args;
    },
    [RequestLanguage.GO] : (opt :GoOption) => {
        const args = [];
        if(opt.omit_must_encode){
            args.push("-must-encode=false");
        }
        if(opt.omit_decode_exact) {
            args.push("-decode-exact=false");
        }
        if(opt.omit_visitors) {
            args.push("-visitor=false");
        }
        if(opt.omit_marshal_json) {
            args.push("-marshal-json=false");
        }
        return args;
    },
    [RequestLanguage.C] : (opt :COption) => {
        const args :string[] = ["--file"];
        if(!opt.multi_file) {
            args.push("--single");
        }
        if(opt.omit_error_callback) {
            args.push("--omit-error-callback");
        }
        if(opt.use_memcpy) {
            args.push("--use-memcpy");
        }
        if(opt.zero_copy) {
            args.push("--zero-copy");
        }
        return args;
    },
    [RequestLanguage.RUST] : (opt :RustOption) => {
        const args :string[] = [];
        return args;
    },
    [RequestLanguage.TYPESCRIPT] : (opt :TSOption) => {
        const args :string[] = ["--file"];
        if(opt.javascript){
            args.push("--javascript");
        }
        return args;
    },
    [RequestLanguage.KAITAI_STRUCT] : (opt :CallOption) => {
        const args :string[] = [];
        return args;
    },
    [RequestLanguage.BINARY_MODULE] : (opt :BMGenOption) => {
        const args :string[] = [];
        if(opt.print_instruction){
            args.push("--print-instructions");
        }
        return args;
    },
    [RequestLanguage.CPP_2] : (opt :Cpp2Option) => {
        const args :string[] = [];
        return args;
    },
    [RequestLanguage.RUST_2] : (opt :Rust2Option) => {
        const args :string[] = [];
        if(opt.use_async){
            args.push("--async");
        }
        return args;
    },
})


export function getLanguage<L extends LanguageKey>(traceID :TraceID,sourceCode :string, lang:L,option :LanguageToOptionType[L]) {
    const mgr = factory.getWorker(LanguageToWorkerType[lang as RequestLanguage]);
    const req = mgr.getRequest(traceID,lang,sourceCode);
    const convert = argConverter[lang] as (opt :LanguageToOptionType[L]) => string[];
    req.arguments = convert(option);
    return mgr.doRequest(req);
}

export const loadWorkers = () => {
    WorkerList.forEach((v) => {
        factory.getWorker(v);
    });
}

loadWorkers();

export const getAST = (id :TraceID,sourceCode :string,options :AstOption) => {
    return getLanguage(id,sourceCode,RequestLanguage.JSON_AST,options)
}

export const getDebugAST = (id :TraceID,sourceCode :string,options :CallOption) => {
    return getLanguage(id,sourceCode,RequestLanguage.JSON_DEBUG_AST,options)
}

export const getTokens = (id :TraceID,sourceCode :string,options :CallOption) => {
    return getLanguage(id,sourceCode,RequestLanguage.TOKENIZE,options)
}

export const getCppCode = (id :TraceID,sourceCode :string,options :CppOption) => {
    return getLanguage(id,sourceCode,RequestLanguage.CPP,options)
}

export const getGoCode = (id :TraceID,sourceCode :string,options :GoOption) => {
    return getLanguage(id,sourceCode,RequestLanguage.GO,options)
}

export const getCCode = (id :TraceID,sourceCode :string,options :COption) => {
    return getLanguage(id,sourceCode,RequestLanguage.C,options)
}

export const getRustCode = (id :TraceID,sourceCode :string,options :RustOption) => {
    return getLanguage(id,sourceCode,RequestLanguage.RUST,options)
}

export const getTSCode = (id :TraceID,sourceCode :string,options :TSOption) => {
    return getLanguage(id,sourceCode,RequestLanguage.TYPESCRIPT,options)
}

export const getKaitaiStructCode = (id :TraceID,sourceCode :string,options :CallOption) => {
    return getLanguage(id,sourceCode,RequestLanguage.KAITAI_STRUCT,options)
}

export const getBinaryModule = (id :TraceID,sourceCode :string,options :BMGenOption) => {
    return getLanguage(id,sourceCode,RequestLanguage.BINARY_MODULE,options)
}

export const getCpp2Code = (id :TraceID,sourceCode :string,options :Cpp2Option) => {
    return getLanguage(id,sourceCode,RequestLanguage.CPP_2,options)
}

export const getRust2Code = (id :TraceID,sourceCode :string,options :Rust2Option) => {
    return getLanguage(id,sourceCode,RequestLanguage.RUST_2,options)
}