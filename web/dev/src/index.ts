
import * as caller from "./s2j/src2json_caller.js";

/// <reference path="../node_modules/monaco-editor/dev/vs/loader.js" />

import * as monaco from "../node_modules/monaco-editor/esm/vs/editor/editor.api.js";

const element = document.getElementById("container");


const editor = monaco.editor.create(element);

(async ()=>{

    const call = await caller.runSrc2JSON(`
format Exec:
    field :u64
`);
    console.log(call);
    const call2 = await caller.runSrc2JSON(`-1 + 1`);
    console.log(call2);
})();
