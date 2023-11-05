
enum RequestMessage {
    MSG_REQUIRE_AST = "MSG_REQUIRE_AST",
    MSG_REQUIRE_TOKENS = "MSG_REQUIRE_TOKENS",
    MSG_REQUIRE_GENERATED_CODE = "MSG_REQUIRE_GENERATED_CODE",
}

interface JobRequest {
    readonly msg :RequestMessage
    readonly jobID :number
    sourceCode? :string
    options? :string[]
}

interface JobResult {
    readonly msg :RequestMessage
    stdout? :string
    stderr? :string
    err? :Error
    code :number
    jobID :number
}

export {RequestMessage,JobRequest,JobResult};