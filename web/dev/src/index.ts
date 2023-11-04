
import * as caller from "./s2j/src2json_caller.js";

/// <reference path="../node_modules/monaco-editor/dev/vs/loader.js" />

import * as monaco from "../node_modules/monaco-editor/esm/vs/editor/editor.api.js";

import {ast2ts} from "../node_modules/ast2ts/index.js";

const container1=  document.getElementById("container1");

if(!container1) throw new Error("container is null");

const container2=  document.getElementById("container2");

if(!container2) throw new Error("container is null");

const title_bar = document.getElementById("title_bar");
if(!title_bar) throw new Error("title_bar is null");

const editor = monaco.editor.create(container1,{
    lineHeight: 20,
    automaticLayout: true,
    theme: "vs-dark",
    colorDecorators: true,
});

const generated = monaco.editor.create(container2,{
    automaticLayout: true,
    readOnly: true,
    theme: "vs-dark",
    colorDecorators: true,
})

const generated_str = "(generated code)";
const generated_model = monaco.editor.createModel(generated_str,"text/plain");
const setDefault = () => {
    generated.setModel(generated_model);
};
setDefault();

// set style
container1.style.position = "absolute";
container1.style.display = "block";
container2.style.position = "absolute";
container2.style.display = "block";
title_bar.style.position = "absolute";
title_bar.style.display = "block";
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

const createGenerated =async (json :string) => {
    const brgen = monaco.editor.createModel(json,"javascript");
    generated.setModel(brgen);
}


window.addEventListener("keydown",async (e) => {
    if(e.ctrlKey&&e.key==="s"){
        e.preventDefault();
        const text = editor.getValue();
        const s =await caller.getAST(text);
        console.log(s);
        if(s.stdout===undefined) throw new Error("stdout is undefined");
        const js = JSON.parse(s.stdout);
        console.log(js);
        createGenerated(JSON.stringify(js,null,4));
    }
});

monaco.languages.register({ id: 'brgen' });
const model =  monaco.editor.createModel("", "brgen");

model.onDidChangeContent(async (e)=>{
    e.changes.forEach((change)=>{
        console.log(change);
    });
    const s = await caller.getAST(model.getValue());
    if(s.stdout===undefined) throw new Error("stdout is undefined");
    console.log(s.stdout);
    const js = JSON.parse(s.stdout);
    if(ast2ts.isAstFile(js)){
        const ts = ast2ts.parseAST(js.ast);
        console.log(ts);
    }
})

editor.setModel(model);
