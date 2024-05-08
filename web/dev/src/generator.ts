
import * as caller from "./s2j/caller";
import { TraceID } from "./s2j/job_mgr";
import { UpdateTracer } from "./s2j/update";
import * as inc from "./cpp_include";
import  { COption, CallOption, CppOption, GoOption, JobResult,Language, RustOption, TSOption } from "./s2j/msg.js";
import {ast2ts} from "ast2ts";
import {storage} from "./storage";
import {ConfigKey} from "./types";
//import { compileCpp } from "./compiler-explorer/api";

export interface UIModel {
    getValue():string;
    setDefault():void;
    setGenerated(s :string,lang :string):void;
    getLanguageConfig(lang :Language, key :ConfigKey):any;
    mappingCode(mappingInfo :MappingInfo[],s :JobResult,lang :Language,offset :number):void;
}

export interface MappingInfo {
    loc :ast2ts.Loc,
    line :number,
}

const isMappingInfo = (obj :any) :obj is MappingInfo => {
    if(ast2ts.isLoc(obj.loc) && obj.line&&typeof obj.line === 'number'){
        return true;
    }
    return false;
}

const isMappingInfoArray = (obj :any) :obj is MappingInfo[] => {
    if(Array.isArray(obj)){
        for(let i = 0;i<obj.length;i++){
            if(!isMappingInfo(obj[i])){
                return false;
            }
        }
        return true;
    }
    return false;
}

const isMappingInfoStruct = (obj :any) :obj is {line_map :MappingInfo[]} => {
    if(obj&&obj.line_map&&isMappingInfoArray(obj.line_map)){
        return true;
    }
    return false;
}


// returns true if updated
const handleLanguage = async (ui :UIModel,s :JobResult,generate:(id :TraceID,src :string,option :any)=>Promise<JobResult>,lang :Language,view_lang: string,option :any) => {
    if(s.stdout===undefined) throw new Error("stdout is undefined");
    const res = await generate(s.traceID,s.stdout,option).catch((e) => {
        return e as JobResult;
    });
    if(updateTracer.editorAlreadyUpdated(s)) {
        return;
    }
    console.log(res);
    if(res.stdout === undefined || res.stdout === "") {
        if(res.stderr!==undefined&&res.stderr!==""){
            ui.setGenerated(res.stderr,"text/plain");
        }
        else{
            ui.setGenerated("(no output. maybe generator is crashed)","text/plain");
        }
    }
    else{
        ui.setGenerated(res.stdout,view_lang);
    }
    return;
}

const handleCppPrototype = async (ui :UIModel,s :JobResult) => {
    const option :CallOption= {};
    return handleLanguage(ui,s,caller.getCppPrototypeCode,Language.CPP_PROTOTYPE,"cpp",option);
}


const SEPARATOR= "############";

const handleCpp = async (ui :UIModel,  s :JobResult) => {
    const useMap = ui.getLanguageConfig(Language.CPP,ConfigKey.CPP_SOURCE_MAP);
    const expandInclude = ui.getLanguageConfig(Language.CPP,ConfigKey.CPP_EXPAND_INCLUDE); 
    const useError = ui.getLanguageConfig(Language.CPP,ConfigKey.CPP_USE_ERROR);
    const useRawUnion = ui.getLanguageConfig(Language.CPP,ConfigKey.CPP_USE_RAW_UNION);
    const checkOverflow = ui.getLanguageConfig(Language.CPP,ConfigKey.CPP_CHECK_OVERFLOW);
    const compileViaAPI = ui.getLanguageConfig(Language.CPP,ConfigKey.CPP_COMPILE_VIA_API);
    const cppOption : CppOption = {      
        use_line_map: useMap === true,
        use_error: useError === true,
        use_raw_union: useRawUnion === true,
        use_overflow_check: checkOverflow === true,
    };
    let result : JobResult | undefined = undefined;
    let mappingInfo :any;
    await handleLanguage(ui,s,async(id: TraceID,src :string,option :any) => {
        result = await caller.getCppCode(id,src,option as CppOption);
        if(result.code === 0&&cppOption.use_line_map){
           const split = result.stdout!.split(SEPARATOR);
           result.stdout = split[0];
           mappingInfo = JSON.parse(split[1]);
        }
        if(result.code === 0&&expandInclude===true){
            const expanded = await inc.resolveInclude(result.stdout!,(url :string)=>{
                if(updateTracer.editorAlreadyUpdated(s)) return;
                ui.setGenerated(`maybe external server call is delayed\nfetching ${url}`,"text/plain");
            });
            result.stdout = expanded;
        }
        return result;
    },Language.CPP,"cpp",cppOption);
    // NOTE(on-keyday):
    // TypeScript may not infer that `result` will be assigned within the async function below.
    // Therefore, we explicitly cast `result` as `JobResult | undefined` to avoid type errors.
    result = result as JobResult | undefined;
    if(result&&!updateTracer.editorAlreadyUpdated(result)){
        if(isMappingInfoStruct(mappingInfo)) {
            // wait for editor update 
            setTimeout(() => {
                if(result===undefined) throw new Error("result is undefined");
                if(updateTracer.editorAlreadyUpdated(s)) return;
                console.log(mappingInfo);
                ui.mappingCode(mappingInfo.line_map,s,Language.CPP,0);
            },1);
        }
        /*
        if(compileViaAPI===true) {
            compileCpp(result.stdout!).then((res)=>{
                console.log(res);
            })
        }
        */
    }    
}

const handleGo = async (ui :UIModel, s :JobResult) => {
    const omitMustEncode = ui.getLanguageConfig(Language.GO,ConfigKey.GO_OMIT_MUST_ENCODE);
    const omitDecodeExact = ui.getLanguageConfig(Language.GO,ConfigKey.GO_OMIT_DECODE_EXACT);
    const goOption : GoOption ={
        omit_decode_exact: omitDecodeExact === true,
        omit_must_encode: omitMustEncode === true,
    }
    return handleLanguage(ui,s,caller.getGoCode,Language.GO,"go",goOption);
}

const handleC = async (ui :UIModel, s :JobResult) => {
    const multiFile = ui.getLanguageConfig(Language.C,ConfigKey.C_MULTI_FILE);
    const omitError = ui.getLanguageConfig(Language.C,ConfigKey.C_OMIT_ERROR_CALLBACK);
    const useMemcpy = ui.getLanguageConfig(Language.C,ConfigKey.C_USE_MEMCPY);
    const zeroCopy = ui.getLanguageConfig(Language.C,ConfigKey.C_ZERO_COPY);
    const COption : COption = {
        multi_file: multiFile === true,
        omit_error_callback: omitError === true,
        use_memcpy: useMemcpy === true,
        zero_copy: zeroCopy === true,
    };
    return handleLanguage(ui,s,async(id,src,option)=>{
        const res = await caller.getCCode(id,src,option as COption);
        if(res.code === 0&&multiFile===true){
            const split = res.stdout!.split(SEPARATOR).join("\n");
            res.stdout = split;
        }
        return res;
    },Language.C,"c",COption);
}

const handleRust = async (ui :UIModel, s :JobResult) => {
    const rustOption : RustOption = {};
    return handleLanguage(ui,s,caller.getRustCode,Language.RUST,"rust",rustOption);
}

const handleTypeScript = async (ui :UIModel, s :JobResult) => {
    const isJavascript = ui.getLanguageConfig(Language.TYPESCRIPT,ConfigKey.TS_JAVASCRIPT);
    const tsOption : TSOption = {
        javascript: isJavascript === true,
    };
    return handleLanguage(ui,s,caller.getTSCode,Language.TYPESCRIPT,
       isJavascript?"javascript" : "typescript",tsOption);
}

const handleKaitaiStruct = async (ui :UIModel, s :JobResult) => {
    const option :CallOption= {};
    return handleLanguage(ui,s,caller.getKaitaiStructCode,Language.KAITAI_STRUCT,"yaml",option);
}

const handleJSONOutput = async (ui :UIModel,id :TraceID,value :string,generator:(id :TraceID,srcCode :string,option:any)=>Promise<JobResult>) => {
    const s = await generator(id,value,
    {filename: "editor.bgn"}).catch((e) => {
        return e as JobResult;
    });
    if(updateTracer.editorAlreadyUpdated(s)) {
        return;
    }
    if(s.stdout===undefined) throw new Error("stdout is undefined");
    console.log(s.stdout);
    console.log(s.stderr);
    const js = JSON.parse(s.stdout);
    ui.setGenerated(JSON.stringify(js,null,4),"json");
    return;
}

const handleTokenize = async (ui :UIModel, id :TraceID,value :string) => {
    return handleJSONOutput(ui,id,value,caller.getTokens);
}

const handleDebugAST = async (ui :UIModel, id :TraceID,value :string) => {
    return handleJSONOutput(ui,id,value,caller.getDebugAST);
}



export const updateTracer = new UpdateTracer();


export const updateGenerated = async (ui :UIModel) => {
    const value = ui.getValue();
    const traceID = updateTracer.getTraceID();
    if(value === ""){
        ui.setDefault();
        return;
    }
    const lang = storage.getLangMode();
    if(lang === Language.TOKENIZE) {
       return handleTokenize(ui,traceID,value);
    }
    if(lang === Language.JSON_DEBUG_AST) {
        return handleDebugAST(ui,traceID,value);
    }
    const s = await caller.getAST(traceID,value,
    {filename: "editor.bgn"}).catch((e) => {
        return e as JobResult;
    });
    if(updateTracer.editorAlreadyUpdated(s)) {
        return;
    }
    if(s.stdout===undefined) throw new Error("stdout is undefined");
    console.log(s.stdout);
    console.log(s.stderr);
    const js = JSON.parse(s.stdout);
    if(ast2ts.isAstFile(js)&&js.ast){
        const ts = ast2ts.parseAST(js.ast);
        console.log(ts);
    }
    switch(lang){
        case Language.JSON_AST:
            ui.setGenerated(JSON.stringify(js,null,4),"json");
            return;
        case Language.CPP_PROTOTYPE: 
            return handleCppPrototype(ui,s);
        case Language.CPP:
            return handleCpp(ui,s);
        case Language.GO:
            return handleGo(ui,s);
        case Language.C:
            return handleC(ui,s);
        case Language.RUST:
            return handleRust(ui,s);
        case Language.TYPESCRIPT:
            return handleTypeScript(ui,s);
        case Language.KAITAI_STRUCT:
            return handleKaitaiStruct(ui,s);
    }
}
