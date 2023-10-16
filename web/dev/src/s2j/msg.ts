
enum RequestMessage {
    MSG_REQUIRE_AST = "MSG_REQUIRE_AST",
    MSG_REQUIRE_TOKENS = "MSG_REQUIRE_TOKENS",
}

interface JobRequest {
    readonly msg :RequestMessage
    readonly jobID :number
    sourceCode? :string
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