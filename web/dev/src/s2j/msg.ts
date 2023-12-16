

enum RequestLanguage {
    TOKENIZE = "tokens",
    JSON_AST = "json ast",
    JSON_DEBUG_AST = "json ast (debug)",
    CPP_PROTOTYPE = "cpp (prototype)",
    CPP = "cpp",
    GO = "go",
}

const LanguageList = [
    RequestLanguage.TOKENIZE,
    RequestLanguage.JSON_AST,
    RequestLanguage.JSON_DEBUG_AST,
    RequestLanguage.CPP_PROTOTYPE,
    RequestLanguage.CPP,
    RequestLanguage.GO,
];


interface JobRequest {
    readonly lang :RequestLanguage
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
    jobID :number
}

export {RequestLanguage as Language,RequestLanguage,LanguageList,JobRequest,JobResult};