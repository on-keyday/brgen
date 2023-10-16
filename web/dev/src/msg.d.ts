
interface MyEmscriptenModule extends EmscriptenModule {
    ccall :typeof ccall
    cwrap :typeof cwrap
}

declare enum ReqestMessage {
    MSG_SOURCE_CODE = "MSG_SOURCE_CODE",
    MSG_RUN = "MSG_RUN"
}

interface JobRequest {
    msg :ReqestMessage
    jobID :number
    sourceCode? :string
}

interface JobResult {
    stdout? :string
    stderr? :string
    code :number
    jobID :number
}