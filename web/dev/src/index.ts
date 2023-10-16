
import * as caller from "./s2j/src2json_caller.js";

/// <reference path="../node_modules/monaco-editor/dev/vs/loader.js" />

import * as monaco from "../node_modules/monaco-editor/esm/vs/editor/editor.api.js";

import {ast2ts} from "../node_modules/ast2ts/index.js";



const container=  document.getElementById("container");

if(!container) throw new Error("container is null");


const editor = monaco.editor.create(container,{
    lineHeight: 20,
    automaticLayout: true,
});


const setWindowSize = () => {
    const h = window.innerHeight;
    const w = window.innerWidth;

    container.style.height = `${h}px`;
    container.style.width = `${w}px`;

    editor.layout({
        width: w,
        height: h,
    });
};

setWindowSize();

window.onresize = setWindowSize;

window.addEventListener("keydown",async (e) => {
    if(e.ctrlKey&&e.key==="s"){
        e.preventDefault();
        const text = editor.getValue();
        const s =await caller.getAST(text);
        console.log(s);
    }
});


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


monaco.languages.register({ id: 'brgen' });


editor.setModel(model);
