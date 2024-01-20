
import "../node_modules/destyle.css/destyle.min.css";
import * as monaco from "../node_modules/monaco-editor/esm/vs/editor/editor.api.js";
import "../node_modules/monaco-editor/esm/vs/language/json/monaco.contribution.js"
import "../node_modules/monaco-editor/esm/vs/basic-languages/cpp/cpp.contribution.js"
import "../node_modules/monaco-editor/esm/vs/basic-languages/go/go.contribution.js"
import {ast2ts} from "../node_modules/ast2ts/index.js";
import * as caller from "./s2j/caller.js";
import "./s2j/brgen_lsp.js";
import { JobResult,Language,LanguageList } from "./s2j/msg.js";
import { makeButton, makeLink, makeListBox, setStyle, makeInputList, InputListElement } from "./ui";

import * as inc from "./cpp_include";
import { TraceID } from "./s2j/job_mgr";


// first, load workers
caller.loadWorkers();

if(window.MonacoEnvironment === undefined) {
    window.MonacoEnvironment = {
        getWorker: (moduleId, label) => {
            if (label === 'json') {
                return new Worker(new URL('../node_modules/monaco-editor/esm/vs/language/json/json.worker',import.meta.url), { type: 'module' });
            }
            return new Worker(new URL('../node_modules/monaco-editor/esm/vs/editor/editor.worker',import.meta.url), { type: 'module' });
        },
    }
}

const enum StorageKey {
    LANG_MODE = "lang_mode",
    SOURCE_CODE = "source_code",
    LANG_SPECIFIC_OPTION = "lang_specific_option",
}

const enum ElementID {
    CONTAINER1 = "container1",
    CONTAINER2 = "container2",
    TITLE_BAR = "title_bar",
    LANGUAGE_SELECT = "language-select",
    COPY_BUTTON = "copy-button",
    GITHUB_LINK = "github-link",
    BALL = "ball",
    BALL_BOUND = "ball-bound",
    INPUT_LIST = "input-list",
}

const enum ConfigKey {
    COMMON_FILE_NAME ="file_name",
    CPP_SOURCE_MAP = "source_map", 
    CPP_EXPAND_INCLUDE = "expand_include",
    CPP_USE_ERROR = "use_error",
    CPP_USE_RAW_UNION = "use_raw_union",
    GO_USE_PUT = "use_put",
}

interface LanguageConfig{
    box: HTMLElement,
    config: Map<string,InputListElement>
};

interface MappingInfo {
    loc :ast2ts.Loc,
    line :number,
}

interface ColorMap {
    info :MappingInfo,
    color :string,
    sourceElement :HTMLElement|undefined,
    generatedElement :HTMLElement|undefined
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

const sample =`
format WebSocketFrame:
    FIN :u1
    RSV1 :u1
    RSV2 :u1
    RSV3 :u1
    Opcode :u4
    Mask :u1
    PayloadLength :u7
    match PayloadLength:
        126 => ExtendedPayloadLength :u16
        127 => ExtendedPayloadLength :u64
    
    if Mask == 1:
        MaskingKey :u32
    
    Payload :[available(ExtendedPayloadLength) ? ExtendedPayloadLength : PayloadLength]u8
`

const storage  = globalThis.localStorage;
const getLangMode = () => {
    const mode = storage.getItem(StorageKey.LANG_MODE);
    if(mode === null) return Language.CPP;
    if(LanguageList.includes(mode as Language)){
        return mode as Language;
    }
    return Language.CPP;
}

const getSourceCode = () => {
    const code = storage.getItem(StorageKey.SOURCE_CODE);
    if(code === null) return sample;
    return code;
}

let options = {
    language_mode: getLangMode(),

    setLanguageMode: (mode :Language) => {
        options.language_mode = mode;
        storage.setItem(StorageKey.LANG_MODE,mode);
    },

    setSourceCode: (code :string) => {
        storage.setItem(StorageKey.SOURCE_CODE,code);
    }
} 

const getElement = (id :string) => {
    const e = document.getElementById(id);
    if(!e) throw new Error(`${id} is null`);
    return e;
}

const title_bar = getElement(ElementID.TITLE_BAR);
const container1 = getElement(ElementID.CONTAINER1);
const container2 = getElement(ElementID.CONTAINER2);

setStyle(container1);
setStyle(container2);
setStyle(title_bar);
title_bar.innerText = "Brgen Web Playground";
title_bar.style.textAlign = "center";
const editor_model = monaco.editor.createModel("", "brgen");
const editorUI = {
    container1: container1,
    container2: container2,
    editorModel: editor_model,
    editor: monaco.editor.create(container1,{
        model: editor_model,
        lineHeight: 20,
        automaticLayout: true,
        colorDecorators: true,
        theme: "brgen-theme",
        "semanticHighlighting.enabled": true,
    }),
    generated: monaco.editor.create(container2,{
        lineHeight: 20,
        automaticLayout: true,
        readOnly: true,
        colorDecorators: true,
    }),
    generated_model: monaco.editor.createModel("(generated code)","text/plain"),
    setDefault: () => {
        editorUI.generated.setModel(editorUI.generated_model);
    },
} as const;



// to disable cursor
editorUI.generated.onDidChangeModel(async (e) => {
    const area = container2.getElementsByTagName("textarea");
    for(let i = 0;i<area.length;i++){
        area[i].style.display = "none";
        area[i].hidden = true;
    }
});


editorUI.setDefault();



const setWindowSize = () => {
    const h =document.documentElement.clientHeight;
    const w = document.documentElement.clientWidth;
    // const h = window.innerHeight;
    // const w = window.innerWidth;

    // title_bar has 10% height of window
    const title_bar_height = h*0.1;
    title_bar.style.height = `${title_bar_height}px`;
    title_bar.style.width = `${w}px`;
    // font size is decided by title_bar_height and title_bar_width
    const title_bar_font_size_1 = title_bar_height*0.5;
    const title_bar_font_size_2 = w*0.05;
    const title_bar_font_size = Math.min(title_bar_font_size_1,title_bar_font_size_2);
    title_bar.style.fontSize = `${title_bar_font_size}px`;

    const editor_height = h - title_bar_height;
    const editor_width = w/2;

    // edit view
    container1.style.top = `${title_bar_height}px`;
    container1.style.height = `${editor_height}px`;
    container1.style.width = `${editor_width}px`;

    // generated view
    container2.style.top = `${title_bar_height}px`;
    container2.style.height = `${editor_height}px`;
    container2.style.width = `${editor_width}px`;
    container2.style.left = `${editor_width}px`;

    editorUI.editor.layout({
        width: editor_width,
        height: editor_height,
    });

    editorUI.generated.layout({
        width: editor_width,
        height: editor_height,
    });

    console.log(`h: ${h}, w: ${w}`);
    console.log(`title_bar_height: ${title_bar_height}`);
    console.log(`title_bar_font_size: ${title_bar_font_size}`);
    console.log(`editor_height: ${editor_height}`);
    console.log(`editor_width: ${editor_width}`);
};

setWindowSize();

window.addEventListener("resize",setWindowSize);

const setGenerated =async (code :string,lang: string) => {
    const top = editorUI.generated.getScrollTop();
    const model = monaco.editor.createModel(code,lang);
    editorUI.generated.setModel(model);
    editorUI.generated.setScrollTop(top);
}




const updateTracer = {
    traceID: 0,
    getTraceID: () =>{
        return updateTracer.traceID++;
    },
    editorAlreadyUpdated: (s :JobResult) => {
        if(updateTracer.traceID !== s.traceID){
            console.log(`already updated traceID: ${s.traceID} jobID: ${s.jobID}`);
            return true;
        }
        return false;
    }
};

// returns true if updated
const handleLanguage = async (s :JobResult,generate:(id :TraceID,src :string,option :any)=>Promise<JobResult>,lang :Language,view_lang: string,option? :any) => {
    if(s.stdout===undefined) throw new Error("stdout is undefined");
    const res = await generate(s.traceID,s.stdout,option).catch((e) => {
        return e as JobResult;
    });
    if(updateTracer.editorAlreadyUpdated(s)) {
        return false;
    }
    if(lang!==options.language_mode) {
        return false;
    }
    console.log(res);
    if(res.stdout === undefined || res.stdout === "") {
        if(res.stderr!==undefined&&res.stderr!==""){
            setGenerated(res.stderr,"text/plain");
        }
        else{
            setGenerated("(no output. maybe generator is crashed)","text/plain");
        }
    }
    else{
        setGenerated(res.stdout,view_lang);
    }
    return true;
}

const handleCppPrototype = async (s :JobResult) => {
    await handleLanguage(s,caller.getCppPrototypeCode,Language.CPP_PROTOTYPE,"cpp");
}

const caches = {
    recoloring: null as null|(()=>void),
}

const mappingCode = (mappingInfo :MappingInfo[],origin :JobResult,lang :Language,count :number) => {
   
    // HACK: these elements are dependent on monaco-editor's implementation and may be changed in the future
    const source_line = editorUI.container1.getElementsByClassName("view-lines");
    const generated_line = editorUI.container2.getElementsByClassName("view-lines");
    const source_line_numbers_element = editorUI.container1.getElementsByClassName("line-numbers");
    const generated_line_numbers_element = editorUI.container2.getElementsByClassName("line-numbers");
    if(generated_line_numbers_element?.length === 0) {
        if(count > 10) {
            console.log(`coloring timeout: traceID: ${origin.traceID} jobID ${origin.jobID}`);
            return;
        }
        // wait for monaco-editor's update
        setTimeout(() => {
            if(updateTracer.editorAlreadyUpdated(origin)) return;
            mappingCode(mappingInfo,origin,lang,count+1);
        },1);
        return;
    }

    const generated_model = editorUI.generated.getModel();
    if(!generated_model) throw new Error("generated model is null");
    if(source_line.length!==1) throw new Error("source line not found");
    if(generated_line.length!==1) throw new Error("generated line not found");
    const source_line_element = source_line[0];
    const generated_line_element = generated_line[0];
    const source_lines = source_line_element.getElementsByClassName("view-line");
    const generated_lines = generated_line_element.getElementsByClassName("view-line");
    console.log(source_line_element);
    console.log(generated_line_element);
    console.log(source_lines);
    console.log(generated_lines);
    console.log(source_line_numbers_element);
    console.log(generated_line_numbers_element);
    const sourceMap = new Map<number,HTMLElement>();
    const generatedMap = new Map<number,HTMLElement>();
    //get inner text of each line number and then make it number
    // if inner text is not number, then ignore it
    const source_line_numbers = Array.from(source_line_numbers_element).map((e) => {
        const text = (e as HTMLElement).innerText;
        const num = Number.parseInt(text,10);
        if(isNaN(num)) return -1;
        sourceMap.set(num,e as HTMLElement);
        return num;
    }).filter((e) => e!==-1);
    const generated_line_numbers = Array.from(generated_line_numbers_element).map((e) => {
        const text = (e as HTMLElement).innerText;
        const num = Number.parseInt(text,10);
        if(isNaN(num)) return -1;
        generatedMap.set(num,e as HTMLElement);
        return num;
    }).filter((e) => e!==-1);
    if(source_line_numbers.length !== source_lines.length) throw new Error("source line number and source line element is not matched");
    if(generated_line_numbers.length !== generated_lines.length) throw new Error("generated line number and generated line element is not matched");
    const source_line_limits = {
        max: Math.max(...source_line_numbers),
        min: Math.min(...source_line_numbers),
    }
    const generated_line_limits = {
        max: Math.max(...generated_line_numbers),
        min: Math.min(...generated_line_numbers),
    }
    Array.from(source_lines).forEach((e) => {
        sourceMap.set(source_line_numbers.shift()!,e as HTMLElement);
    })
    Array.from(generated_lines).forEach((e) => {
        generatedMap.set(generated_line_numbers.shift()!,e as HTMLElement);
    })
    console.log(source_line_numbers);
    console.log(source_line_limits);
    console.log(generated_line_numbers);
    console.log(generated_line_limits);
    console.log(sourceMap);
    console.log(generatedMap);
    const colors = [
        "rgba(255, 0, 0, 0.5)",// red
        "rgba(0,0,255,0.5)",// blue
        "rgba(255,255,0,0.5)",// yellow
        "rgba(0,255,0,0.5)",// green
        "rgba(255,0,255,0.5)",// purple
        "rgba(255,165,0,0.5)",// orange
    ];
    const useMapping = mappingInfo.map((e) => {
        const source = sourceMap.get(e.loc.line);
        const generated = generatedMap.get(e.line);
        if(source === undefined && generated === undefined) return null;
        const color = colors[e.loc.line%colors.length];
        return {
            info: e,
            color: color,
            sourceElement: source,
            generatedElement: generated,
        } as ColorMap;
    }).filter((e) => e!==null) as ColorMap[]; 
    console.log(useMapping);
    useMapping.forEach((e) => {
        if(e.sourceElement) {
            e.sourceElement.style.backgroundColor = e.color;
        }
        if(e.generatedElement) {
            e.generatedElement.style.backgroundColor = e.color;
        }
    });
    const recoloring = () => {
        observer1.disconnect();
        observer2.disconnect();
        const mapping = commonUI.config.get(lang)?.config.get(ConfigKey.CPP_SOURCE_MAP)?.value === true;
        if(updateTracer.editorAlreadyUpdated(origin)||lang !== options.language_mode||!mapping)  {
            useMapping.forEach((e) => {
                if(e.sourceElement) {
                    e.sourceElement.style.backgroundColor = '';
                }
                if(e.generatedElement) {
                    e.generatedElement.style.backgroundColor = '';
                }
            });
            if(caches.recoloring === recoloring) {
                caches.recoloring = null;
            }
            return;
        }
        mappingCode(mappingInfo,origin,lang,0);// recursive call
    };
    const observer1 = new MutationObserver(recoloring);
    observer1.observe(source_line_element,{
        childList: true,
        subtree: true,
    });
    const observer2 = new MutationObserver(recoloring);
    observer2.observe(generated_line_element,{
        childList: true,
        subtree: true,
    });
    caches.recoloring = recoloring;
}

const handleCpp = async (s :JobResult) => {
    const useMap =commonUI.config.get(Language.CPP)?.config.get(ConfigKey.CPP_SOURCE_MAP)?.value;
    const expandInclude = commonUI.config.get(Language.CPP)?.config.get(ConfigKey.CPP_EXPAND_INCLUDE)?.value;
    const useError = commonUI.config.get(Language.CPP)?.config.get(ConfigKey.CPP_USE_ERROR)?.value;
    const useRawUnion = commonUI.config.get(Language.CPP)?.config.get(ConfigKey.CPP_USE_RAW_UNION)?.value;
    const cppOption : caller.CppOption = {      
        use_line_map: useMap === true,
        use_error: useError === true,
        use_raw_union: useRawUnion === true,
    };
    let result : JobResult | undefined = undefined;
    let mappingInfo :any;
    const updated = await handleLanguage(s,async(id: TraceID,src :string,option :any) => {
        result = await caller.getCppCode(id,src,option as caller.CppOption);
        if(result.code === 0&&cppOption.use_line_map){
           const split = result.stdout!.split("############");
           result.stdout = split[0];
           mappingInfo = JSON.parse(split[1]);
        }
        if(result.code === 0&&expandInclude===true){
            const expanded = await inc.resolveInclude(result.stdout!,(url :string)=>{
                setGenerated(`maybe external server call is delayed\nfetching ${url}`,"text/plain");
            });
            result.stdout = expanded;
        }
        return result;
    },Language.CPP,"cpp",cppOption);
    if(updated&& isMappingInfoArray(mappingInfo?.line_map)){
        // wait for editor update 
        setTimeout(() => {
            if(result===undefined) throw new Error("result is undefined");
            if(updateTracer.editorAlreadyUpdated(s)) return;
            console.log(mappingInfo);
            mappingCode(mappingInfo.line_map,s,Language.CPP,0);
        },1);
    }
}

const handleGo = async (s :JobResult) => {
    const usePut = commonUI.config.get(Language.GO)?.config.get(ConfigKey.GO_USE_PUT)?.value;
    const goOption : caller.GoOption ={
        use_put: usePut === true,
    }
    await handleLanguage(s,caller.getGoCode,Language.GO,"go",goOption);
}

const handleJSONOutput = async (id :TraceID,value :string,generator:(id :TraceID,srcCode :string,option:any)=>Promise<JobResult>) => {
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
    setGenerated(JSON.stringify(js,null,4),"json");
    return;
}

const handleTokenize = async (id :TraceID,value :string) => {
    await handleJSONOutput(id,value,caller.getTokens);
}

const handleDebugAST = async (id :TraceID,value :string) => {
    await handleJSONOutput(id,value,caller.getDebugAST);
}


const updateGenerated = async () => {
    const value = editorUI.editor.getValue();
    if(value === ""){
        editorUI.setDefault();
        return;
    }
    const traceID = updateTracer.getTraceID();
    if(options.language_mode === Language.TOKENIZE) {
       return await handleTokenize(traceID,value);
    }
    if(options.language_mode === Language.JSON_DEBUG_AST) {
        return await handleDebugAST(traceID,value);
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
    switch(options.language_mode){
        case Language.JSON_AST:
            setGenerated(JSON.stringify(js,null,4),"json");
            break;
        case Language.CPP_PROTOTYPE: 
            await handleCppPrototype(s);
            break;
        case Language.CPP:
            await handleCpp(s);
            break;
        case Language.GO:
            await handleGo(s);
            break;
    }
}

window.addEventListener("keydown",async (e) => {
    if(e.ctrlKey&&e.key==="s"){
        e.preventDefault();
        updateGenerated();
    }
});

// monaco.languages.register({ id: 'brgen' });


const onContentUpdate = async (e :monaco.editor.IModelContentChangedEvent)=>{
    e.changes.forEach((change)=>{
        console.log(change);
    });
    options.setSourceCode(editorUI.editorModel.getValue());
    await updateGenerated();
}

editorUI.editorModel.onDidChangeContent(onContentUpdate)

interface Internal {
    languageElement :HTMLElement|null;
}

const changeLanguage = async (mode :string) => {
    if(!LanguageList.includes(mode as Language)) {
        throw new Error(`invalid language mode: ${mode}`);
    }
    commonUI.language_select.value = mode;
    options.setLanguageMode(mode as Language);
    commonUI.changeLanguageConfig(mode as Language);
    await updateGenerated();
}

const commonUI = {
    title_bar: title_bar,
    language_select: makeListBox(ElementID.LANGUAGE_SELECT,LanguageList,
        options.language_mode,
        async () => {
            const value = commonUI.language_select.value;
            await changeLanguage(value);
        },
        {
            top: "50%",
            left: "80%",
            fontSize: "60%",
            border: "solid 1px black",
        }),
    copy_button: makeButton(ElementID.COPY_BUTTON,"copy code",async () => {
            const code = editorUI.generated.getValue();
            if(navigator.clipboard===undefined){
            commonUI.copy_button.innerText = "not supported";
                setTimeout(() => {
                    commonUI.copy_button.innerText = "copy code";
                },1000);
                return;
            }
            commonUI.copy_button.innerText = "copied!";
            setTimeout(() => {
                commonUI.copy_button.innerText = "copy code";
            },1000);
            await navigator.clipboard.writeText(code);
        },
        {
            top: "50%",
            left: "65%",
            fontSize: "60%",
            border: "solid 1px black",
        }),
    github_link: makeLink(ElementID.GITHUB_LINK,"develop page","https://github.com/on-keyday/brgen",
        (e) => {
            e.preventDefault();
            location.href = commonUI.github_link.href;
        },
        {
            top: "50%",
            left: "4%",
            fontSize: "60%",
            color: "blue",
        }),
    config: new Map<Language,LanguageConfig>(),  
    
    internal: {
        languageElement: null,
    } as Internal,
    
    changeLanguageConfig: (lang :Language)=>{
        commonUI.internal.languageElement?.remove();
        commonUI.internal.languageElement = null;
        const config = commonUI.config.get(lang);
        if(config){
            commonUI.internal.languageElement = config.box;
            commonUI.title_bar.appendChild(config.box);
        }
        if(caches.recoloring !== null) {
            caches.recoloring();
        }
    }
} as const;
commonUI.title_bar.appendChild(commonUI.language_select);
commonUI.title_bar.appendChild(commonUI.copy_button);
commonUI.title_bar.appendChild(commonUI.github_link);


const languageSpecificConfig = (conf :Map<string,InputListElement>,default_ :string,change: (change: InputListElement)=>void):LanguageConfig => {
    const box = makeInputList(ElementID.INPUT_LIST,conf,default_,change,
    {
        top: "50%",
        left: "25%",
        fontSize: "60%",
        border: "solid 1px black",
    });
    return {
        box: box,
        config: conf,
    }
}

const fileName :InputListElement = {
    "type": "text",
    "value": "editor.bgn",
}

// language specific config
{
    const tokenize = new Map<string,InputListElement>();
    tokenize.set(ConfigKey.COMMON_FILE_NAME,fileName);
    commonUI.config.set(Language.TOKENIZE,languageSpecificConfig(tokenize,ConfigKey.COMMON_FILE_NAME,(change) => {
        updateGenerated();
    }));
    const debugAST = new Map<string,InputListElement>();
    debugAST.set(ConfigKey.COMMON_FILE_NAME,fileName);
    commonUI.config.set(Language.JSON_DEBUG_AST,languageSpecificConfig(debugAST,ConfigKey.COMMON_FILE_NAME,(change) => {
        updateGenerated();
    }));
    const cppProto = new Map<string,InputListElement>();
    cppProto.set(ConfigKey.COMMON_FILE_NAME,fileName);
    commonUI.config.set(Language.CPP_PROTOTYPE,languageSpecificConfig(cppProto,ConfigKey.COMMON_FILE_NAME,(change) => {
        updateGenerated();
    }));
    const go = new Map<string,InputListElement>();
    go.set(ConfigKey.GO_USE_PUT,{
        "type": "checkbox",
        "value": false,
    });
    go.set(ConfigKey.COMMON_FILE_NAME,fileName);
    commonUI.config.set(Language.GO,languageSpecificConfig(go,ConfigKey.COMMON_FILE_NAME,(change) => {
        updateGenerated();
    }));
    const ast = new Map<string,InputListElement>();
    ast.set(ConfigKey.COMMON_FILE_NAME,fileName);
    commonUI.config.set(Language.JSON_AST,languageSpecificConfig(ast,ConfigKey.COMMON_FILE_NAME,(change) => {
        updateGenerated();
    }));
    const cpp = new Map<string,InputListElement>();
    cpp.set(ConfigKey.CPP_SOURCE_MAP,{
        "type": "checkbox",
        "value": false,
    });
    cpp.set(ConfigKey.CPP_EXPAND_INCLUDE,{
        "type": "checkbox",
        "value": false,
    })
    cpp.set(ConfigKey.CPP_USE_ERROR,{
        "type": "checkbox",
        "value": false,
    });
    cpp.set(ConfigKey.CPP_USE_RAW_UNION,{
        "type": "checkbox",
        "value": false,
    });
    cpp.set(ConfigKey.COMMON_FILE_NAME,fileName);
    commonUI.config.set(Language.CPP,languageSpecificConfig(cpp,ConfigKey.CPP_SOURCE_MAP,(change) => {
        if(change.name === ConfigKey.CPP_SOURCE_MAP){
            if(change.value === false) {
                if(caches.recoloring !== null) {
                    caches.recoloring(); 
                    return;
                }
            }
        }
        updateGenerated();
    }));
}

commonUI.changeLanguageConfig(options.language_mode);
editorUI.editorModel.setValue(getSourceCode());

document.getElementById(ElementID.BALL)?.remove();
document.getElementById(ElementID.BALL_BOUND)?.remove();

console.log(monaco.languages.getLanguages());

