


import "../node_modules/destyle.css/destyle.min.css";
import * as monaco from "../node_modules/monaco-editor/esm/vs/editor/editor.api.js";
import {ast2ts} from "../node_modules/ast2ts/index.js";
import * as caller from "./s2j/caller.js";
import { JobResult,Language,LanguageList } from "./s2j/msg.js";
import { makeButton, makeLink, makeListBox, setStyle } from "./ui";

// first, load workers
caller.loadWorkers();

const enum StorageKey {
    LANG_MODE = "lang_mode",
    SOURCE_CODE = "source_code",
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

const container1 = getElement(ElementID.CONTAINER1);

const container2= getElement(ElementID.CONTAINER2);

const title_bar = getElement(ElementID.TITLE_BAR);

const editor = monaco.editor.create(container1,{
    lineHeight: 20,
    automaticLayout: true,
    colorDecorators: true,
});

const generated = monaco.editor.create(container2,{
    lineHeight: 20,
    automaticLayout: true,
    readOnly: true,
    colorDecorators: true,
});

// to disable cursor
generated.onDidChangeModel(async (e) => {
    const area = container2.getElementsByTagName("textarea");
    for(let i = 0;i<area.length;i++){
        area[i].style.display = "none";
        area[i].hidden = true;
    }
});

const generated_model = monaco.editor.createModel("(generated code)","text/plain");
const setDefault = () => {
    generated.setModel(generated_model);
};
setDefault();



setStyle(container1);
setStyle(container2);
setStyle(title_bar);
title_bar.innerText = "Brgen Web Playground";
title_bar.style.textAlign = "center";


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

    editor.layout({
        width: editor_width,
        height: editor_height,
    });

    generated.layout({
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

window.onresize = setWindowSize;

const createGenerated =async (code :string,lang: string) => {
    const model = monaco.editor.createModel(code,lang);
    generated.setModel(model);
}

const alreadyUpdated = (s :JobResult) => {
    if(s?.originalSourceCode !== editor.getValue()){
        console.log(`already updated jobID: ${s.jobID}`);
        return true;
    }
    return false;
}

const handleLanguage = async (s :JobResult,generate:(src :string,option :any)=>Promise<JobResult>,lang: string,option? :any) => {
    if(s.stdout===undefined) throw new Error("stdout is undefined");
    const res = await generate(s.stdout,option).catch((e) => {
        return e as JobResult;
    });
    if(alreadyUpdated(s)) {
        return;
    }
    console.log(res);
    if(res.stdout === undefined || res.stdout === "") {
        if(res.stderr!==undefined&&res.stderr!==""){
            createGenerated(res.stderr,"text/plain");
        }
        else{
            createGenerated("(no output. maybe generator is crashed)","text/plain");
        }
    }
    else{
        createGenerated(res.stdout,lang);
    }
}

const handleCppPrototype = async (s :JobResult) => {
    await handleLanguage(s,caller.getCppPrototypeCode,"cpp");
}

const handleCpp = async (s :JobResult) => {
    await handleLanguage(s,caller.getCppCode,"cpp");
}

const handleGo = async (s :JobResult) => {
    const goOption : caller.GoOption ={
        use_put: true,
    }
    await handleLanguage(s,caller.getGoCode,"go",goOption);
}

const handleJSONOutput = async (value :string,generator:(srcCode :string,option:any)=>Promise<JobResult>) => {
    const s = await generator(value,
    {filename: "editor.bgn"}).catch((e) => {
        return e as JobResult;
    });
    if(alreadyUpdated(s)) {
        return;
    }
    if(s.stdout===undefined) throw new Error("stdout is undefined");
    console.log(s.stdout);
    console.log(s.stderr);
    const js = JSON.parse(s.stdout);
    createGenerated(JSON.stringify(js,null,4),"json");
    return;
}

const handleTokenize = async (value :string) => {
    await handleJSONOutput(value,caller.getTokens);
}

const handleDebugAST = async (value :string) => {
    await handleJSONOutput(value,caller.getDebugAST);
}

const updateGenerated = async () => {
    const value = editor.getValue();
    if(value === ""){
        setDefault();
        return;
    }
    if(options.language_mode === Language.TOKENIZE) {
       return await handleTokenize(value);
    }
    if(options.language_mode === Language.JSON_DEBUG_AST) {
        return await handleDebugAST(value);
    }
    const s = await caller.getAST(value,
    {filename: "editor.bgn"}).catch((e) => {
        return e as JobResult;
    });
    if(alreadyUpdated(s)) {
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
            createGenerated(JSON.stringify(js,null,4),"json");
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

monaco.languages.register({ id: 'brgen' });
const editor_model =  monaco.editor.createModel("", "brgen");

editor_model.onDidChangeContent(async (e)=>{
    e.changes.forEach((change)=>{
        console.log(change);
    });
    options.setSourceCode(editor_model.getValue());
    await updateGenerated();
})

editor.setModel(editor_model);

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
            const code = generated.getValue();
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
            left: "5%",
            fontSize: "60%",
            color: "blue",
        }),
} as const;



const changeLanguage = async (mode :string) => {
    commonUI.language_select.value = mode;
    if(LanguageList.includes(mode as Language)) {
        options.setLanguageMode(mode as Language);
    }
    else{
        throw new Error(`invalid language mode: ${mode}`);
    }
    await updateGenerated();
}

commonUI.title_bar.appendChild(commonUI.language_select);
commonUI.title_bar.appendChild(commonUI.copy_button);
commonUI.title_bar.appendChild(commonUI.github_link);

editor_model.setValue(getSourceCode());

document.getElementById(ElementID.BALL)?.remove();
document.getElementById(ElementID.BALL_BOUND)?.remove();
