
enum RequestMessage {
    MSG_REQUIRE_AST = "MSG_REQUIRE_AST",
    MSG_REQUIRE_TOKENS = "MSG_REQUIRE_TOKENS",
    MSG_REQUIRE_GENERATED_CODE = "MSG_REQUIRE_GENERATED_CODE",
}

enum RequestLanguage {
    TOKENIZE = "tokens",
    JSON_AST = "json ast",
    JSON_DEBUG_AST = "json ast (debug)",
    CPP_PROTOTYPE = "cpp (prototype)",
    CPP = "cpp",
    GO = "go",
}

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

export {RequestLanguage as Language,RequestLanguage,JobRequest,JobResult};