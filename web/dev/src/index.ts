
import * as caller from "./s2j/caller.js";

/// <reference path="../node_modules/monaco-editor/dev/vs/loader.js" />

import * as monaco from "../node_modules/monaco-editor/esm/vs/editor/editor.api.js";

import {ast2ts} from "../node_modules/ast2ts/index.js";
import { JobResult } from "./s2j/msg.js";

enum Language {
    JSON_AST = "json ast",
    CPP = "cpp",
}

const storage  = window.localStorage;
const getLangMode = () => {
    const mode = storage.getItem("lang_mode");
    if(mode === null) return Language.JSON_AST;
    switch(mode){
        case Language.JSON_AST:
            return Language.JSON_AST;
        case Language.CPP:
            return Language.CPP;
        default:
            return Language.JSON_AST;
    }
}

const getSourceCode = () => {
    const code = storage.getItem("source_code");
    if(code === null) return "";
    return code;
};

let options = {
    language_mode: getLangMode(),

    setLanguageMode: (mode :Language) => {
        options.language_mode = mode;
        storage.setItem("lang_mode",mode);
    },

    setSourceCode: (code :string) => {
        storage.setItem("source_code",code);
    }
}

const getElement = (id :string) => {
    const e = document.getElementById(id);
    if(!e) throw new Error(`${id} is null`);
    return e;
}

const container1 = getElement("container1");

const container2= getElement("container2");

const title_bar = getElement("title_bar");

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

const generated_str = "(generated code)";
const generated_model = monaco.editor.createModel(generated_str,"text/plain");
const setDefault = () => {
    generated.setModel(generated_model);
};
setDefault();

// set style
const setStyle = (e :HTMLElement) => {
    e.style.position = "absolute";
    e.style.display = "block";
}

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

const handleCpp = async (s :JobResult) => {
    if(s.stdout===undefined) throw new Error("stdout is undefined");
    const cpp = await caller.getCppCode(s.stdout).catch((e) => {
        return e as JobResult;
    });
    if(alreadyUpdated(s)) {
        return;
    }
    console.log(cpp);
    if(cpp.stdout === undefined || cpp.stdout === "") {
        if(cpp.stderr!==undefined&&cpp.stderr!==""){
            createGenerated(cpp.stderr,"text/plain");
        }
        else{
            createGenerated("(no output. maybe generator is crashed)","text/plain");
        }
    }
    else{
        createGenerated(cpp.stdout,"cpp");
    }
}

const updateGenerated = async () => {
    const value = editor_model.getValue();
    if(value === ""){
        setDefault();
        return;
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
        case Language.CPP: {
            await handleCpp(s);
            break;
        }
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

const makeListBox = (id :string,items :string[]) => {
    const select = document.createElement("select");
    select.id = id;
    items.forEach((item) => {
        const option = document.createElement("option");
        option.value = item;
        option.innerText = item;
        select.appendChild(option);
    });
    setStyle(select);
    return select;
}

const select = makeListBox("language-select",[Language.JSON_AST,Language.CPP]);
select.value = options.language_mode;
select.style.top = "50%";
select.style.left ="80%";

title_bar.appendChild(select);

select.onchange = async (e) => {
    const value = select.value;
    switch(value){
        case Language.JSON_AST:
            options.setLanguageMode(Language.JSON_AST);
            break;
        case Language.CPP:
            options.setLanguageMode(Language.CPP);
            break;
        default:
            throw new Error(`unknown language ${value}`);
    }
    await updateGenerated();
};

editor_model.setValue(getSourceCode());
caller.loadWorkers();