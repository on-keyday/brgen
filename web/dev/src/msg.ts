
enum RequestMessage {
    MSG_SOURCE_CODE = "MSG_SOURCE_CODE",
    MSG_RUN = "MSG_RUN"
}

interface JobRequest {
    msg :RequestMessage
    readonly jobID :number
    sourceCode? :string
}

interface JobResult {
    stdout? :string
    stderr? :string
    code :number
    jobID :number
}

export {RequestMessage,JobRequest,JobResult};