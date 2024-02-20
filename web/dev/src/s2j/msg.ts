

const enum RequestLanguage {
    TOKENIZE = "tokens",
    JSON_AST = "json ast",
    JSON_DEBUG_AST = "json ast (debug)",
    CPP_PROTOTYPE = "cpp (prototype)",
    CPP = "cpp",
    GO = "go",
    C = "c",
    RUST = "rust",
}

const LanguageList = [
    RequestLanguage.TOKENIZE,
    RequestLanguage.JSON_AST,
    RequestLanguage.JSON_DEBUG_AST,
    RequestLanguage.CPP_PROTOTYPE,
    RequestLanguage.CPP,
    RequestLanguage.GO,
    RequestLanguage.C,
    RequestLanguage.RUST, // not implemented yet
];


interface JobRequest {
    readonly lang :RequestLanguage
    readonly traceID:number|null
    readonly jobID :number
    sourceCode? :string
    arguments? :string[]
}

interface JobResult {
    readonly lang :RequestLanguage
    stdout? :string
    stderr? :string
    originalSourceCode? :string
    err? :Error
    code :number
    readonly jobID :number
    readonly traceID:number|null
}

export {RequestLanguage as Language,RequestLanguage,LanguageList,JobRequest,JobResult};
