import {AstOption, BMGenOption, COption, CallOption, CppOption, EBMGenOption, GoOption, LanguageKey, LanguageToOptionType, LanguageToWorkerType, RequestLanguage, Rust2Option, RustOption, TSOption, WorkerList, WorkerType }  from "./msg.js";
import {JobManager} from "./job_mgr.js";
import {TraceID } from "./msg.js";



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
        if(opt.force_optional_getter){
            args.push("--force-optional-getter");
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
    [RequestLanguage.EBM] : (opt :EBMGenOption) => {
        const args :string[] = [];
        if(opt.print_instruction){
            args.push("--debug-print","-");
        }
        if(!opt.no_output) {
            args.push("--output","-","--base64");
        }
        if(opt.control_flow_graph) {
            args.push("--cfg-output","-");
        }
        return args;
    }
    /*
    [RequestLanguage.CPP_2] : (opt :Cpp2Option) => {
        const args :string[] = [];
        return args;
    },
    [RequestLanguage.RUST_2] : (opt :Rust2Option) => {
        const args :string[] = [];
        if(opt.use_async){
            args.push("--async");
        }
        if(opt.use_copy_on_write_vec){
            args.push("--copy-on-write");
        }
        return args;
    },
    */
})



export interface IWorkerFactory {
    getWorker(lang :WorkerType) :JobManager
}


export function getLanguage<L extends LanguageKey>(factory :IWorkerFactory, traceID :TraceID,sourceCode :string, lang:L,option :LanguageToOptionType[L]) {
    const mgr = factory.getWorker(LanguageToWorkerType[lang as RequestLanguage]);
    const req = mgr.getRequest(traceID,lang,sourceCode);
    const convert = argConverter[lang] as (opt :LanguageToOptionType[L]) => string[];
    req.arguments = convert(option);
    return mgr.doRequest(req);
}



export const getAST = (factory :IWorkerFactory,id :TraceID,sourceCode :string,options :AstOption) => {
    return getLanguage(factory, id,sourceCode,RequestLanguage.JSON_AST,options)
}

export const getDebugAST = (factory :IWorkerFactory,id :TraceID,sourceCode :string,options :CallOption) => {
    return getLanguage(factory,id,sourceCode,RequestLanguage.JSON_DEBUG_AST,options)
}

export const getTokens = (factory :IWorkerFactory,id :TraceID,sourceCode :string,options :CallOption) => {
    return getLanguage(factory,id,sourceCode,RequestLanguage.TOKENIZE,options)
}

export const getCppCode = (factory :IWorkerFactory,id :TraceID,sourceCode :string,options :CppOption) => {
    return getLanguage(factory,id,sourceCode,RequestLanguage.CPP,options)
}

export const getGoCode = (factory :IWorkerFactory,id :TraceID,sourceCode :string,options :GoOption) => {
    return getLanguage(factory,id,sourceCode,RequestLanguage.GO,options)
}

export const getCCode = (factory :IWorkerFactory,id :TraceID,sourceCode :string,options :COption) => {
    return getLanguage(factory,id,sourceCode,RequestLanguage.C,options)
}

export const getRustCode = (factory :IWorkerFactory,id :TraceID,sourceCode :string,options :RustOption) => {
    return getLanguage(factory,id,sourceCode,RequestLanguage.RUST,options)
}

export const getTSCode = (factory :IWorkerFactory,id :TraceID,sourceCode :string,options :TSOption) => {
    return getLanguage(factory,id,sourceCode,RequestLanguage.TYPESCRIPT,options)
}

export const getKaitaiStructCode = (factory :IWorkerFactory,id :TraceID,sourceCode :string,options :CallOption) => {
    return getLanguage(factory,id,sourceCode,RequestLanguage.KAITAI_STRUCT,options)
}

export const getBinaryModule = (factory :IWorkerFactory,id :TraceID,sourceCode :string,options :BMGenOption) => {
    return getLanguage(factory,id,sourceCode,RequestLanguage.BINARY_MODULE,options)
}

export const getEBM = (factory :IWorkerFactory,id :TraceID,sourceCode :string,options :EBMGenOption) => {
    return getLanguage(factory,id,sourceCode,RequestLanguage.EBM,options)
}

/*
export const getCpp2Code = (id :TraceID,sourceCode :string,options :Cpp2Option) => {
    return getLanguage(id,sourceCode,RequestLanguage.CPP_2,options)
}

export const getRust2Code = (id :TraceID,sourceCode :string,options :Rust2Option) => {
    return getLanguage(id,sourceCode,RequestLanguage.RUST_2,options)
}
*/