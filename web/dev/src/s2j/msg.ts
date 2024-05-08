

export const enum RequestLanguage {
    TOKENIZE = "tokens",
    JSON_AST = "json ast",
    JSON_DEBUG_AST = "json ast (debug)",
    CPP_PROTOTYPE = "cpp (prototype)",
    CPP = "cpp",
    GO = "go",
    C = "c",
    RUST = "rust",
    TYPESCRIPT="typescript",
    KAITAI_STRUCT = "kaitai struct",
}

export const LanguageList = [
    RequestLanguage.TOKENIZE,
    RequestLanguage.JSON_AST,
    RequestLanguage.JSON_DEBUG_AST,
    RequestLanguage.CPP_PROTOTYPE,
    RequestLanguage.CPP,
    RequestLanguage.GO,
    RequestLanguage.C,
    RequestLanguage.RUST, 
    RequestLanguage.TYPESCRIPT,
    RequestLanguage.KAITAI_STRUCT
];

export const enum WorkerType {
    SRC2JSON,
    JSON2CPP,
    JSON2CPP2,
    JSON2GO,
    JSON2C,
    JSON2RUST,
    JSON2TS,
    JSON2KAITAI,
}

export const WorkerList = Object.freeze([
    WorkerType.SRC2JSON,
    WorkerType.JSON2CPP,
    WorkerType.JSON2CPP2,
    WorkerType.JSON2GO,
    WorkerType.JSON2C,
    WorkerType.JSON2RUST,
    WorkerType.JSON2TS,
    WorkerType.JSON2KAITAI,
]);


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
    omit_must_encode? :boolean,
    omit_decode_exact? :boolean,
}

export interface COption extends CallOption {
    multi_file? :boolean
    omit_error_callback? :boolean
    use_memcpy? :boolean
    zero_copy? :boolean
}

export interface RustOption extends CallOption {}

export interface TSOption extends CallOption {
    javascript? :boolean
}


export type LanguageToOptionType = {
    [RequestLanguage.TOKENIZE]:CallOption
    [RequestLanguage.JSON_AST]:AstOption
    [RequestLanguage.JSON_DEBUG_AST]:CallOption
    [RequestLanguage.CPP_PROTOTYPE]:CallOption
    [RequestLanguage.CPP]:CppOption
    [RequestLanguage.GO]:GoOption
    [RequestLanguage.C]:COption
    [RequestLanguage.RUST]:RustOption
    [RequestLanguage.TYPESCRIPT]:TSOption
    [RequestLanguage.KAITAI_STRUCT]:CallOption
}

export const LanguageToWorkerType = Object.freeze({
    [RequestLanguage.TOKENIZE]: WorkerType.SRC2JSON,
    [RequestLanguage.JSON_AST]: WorkerType.SRC2JSON,
    [RequestLanguage.JSON_DEBUG_AST]: WorkerType.SRC2JSON,
    [RequestLanguage.CPP_PROTOTYPE]: WorkerType.JSON2CPP,
    [RequestLanguage.CPP]: WorkerType.JSON2CPP2,
    [RequestLanguage.GO]: WorkerType.JSON2GO,
    [RequestLanguage.C]: WorkerType.JSON2C,
    [RequestLanguage.RUST]: WorkerType.JSON2RUST,
    [RequestLanguage.TYPESCRIPT]: WorkerType.JSON2TS,
    [RequestLanguage.KAITAI_STRUCT]: WorkerType.JSON2KAITAI,
})


export type LanguageKey = keyof LanguageToOptionType;

export interface JobRequest {
    readonly lang :RequestLanguage
    readonly traceID:number|null
    readonly jobID :number
    readonly sourceCode :string
    arguments? :string[]
}

export interface JobResult {
    readonly lang :RequestLanguage
    stdout? :string
    stderr? :string
    originalSourceCode? :string
    err? :Error
    code :number
    readonly jobID :number
    readonly traceID:number|null
}

export {RequestLanguage as Language}
