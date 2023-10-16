
const work = new Worker("./lib/invoke_src2json.js");

let jobID = 0;

var resolverMap = new Map<number, {resolve : (value :JobResult) => void, reject : (reason :any) => void}>();

work.onmessage = (e) => {
    const msg = e.data as JobResult;
    const resolver = resolverMap.get(msg.jobID);
    if(resolver){
        if(msg.code===0){
            resolver.resolve(msg);
        }else{
            resolver.reject(msg);
        }
        resolverMap.delete(msg.jobID);
    }
};

export const runSrc2JSON = (sourceCode :string) => {
    return new Promise<JobResult>((resolve,reject) => {
        const id = jobID;
        jobID++;
        const req :JobRequest = {
            msg :ReqestMessage.MSG_SOURCE_CODE,
            jobID :id,
            sourceCode
        };
        resolverMap.set(id,{resolve,reject});
        work.postMessage(req);
    });
}
