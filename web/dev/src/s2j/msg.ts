
enum RequestMessage {
    MSG_REQUIRE_AST = "MSG_REQUIRE_AST",
    MSG_REQUIRE_TOKENS = "MSG_REQUIRE_TOKENS",
    MSG_REQUIRE_GENERATED_CODE = "MSG_REQUIRE_GENERATED_CODE",
}

enum RequestLanguage {
    TOKENIZE = "TOKENIZE",
    JSON_AST = "JSON_AST",
    CPP_PROTOTYPE = "CPP_PROTOTYPE",
    CPP = "CPP",
    GO = "GO",
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

export {RequestLanguage,JobRequest,JobResult};