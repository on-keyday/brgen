
import * as stub from "./inputServiceStub.js"




export interface FileCandidates {
    placeHolder :string;
    candidates :string[];
}

const registerFileSelectionCallback = (candidates :()=>Promise<FileCandidates>,select :(p :string)=>void) => {    
    stub.cacheRef.fileCandidates = candidates;
    stub.cacheRef.fileSelected = (a: any) => {
        select(a?.id);
    };
}

export {registerFileSelectionCallback}
