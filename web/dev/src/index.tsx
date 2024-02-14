
import "../node_modules/destyle.css/destyle.min.css";
import * as monaco from "monaco-editor";

import "monaco-editor/esm/vs/language/json/monaco.contribution"
import "monaco-editor/esm/vs/basic-languages/cpp/cpp.contribution"
import "monaco-editor/esm/vs/basic-languages/go/go.contribution"


import "./s2j/brgen_lsp.js";
import { JobResult,Language,LanguageList } from "./s2j/msg.js";
import { makeButton, makeLink, makeListBox, setStyle, makeInputList, InputListElement } from "./ui";

import {storage} from "./storage";
import { FileCandidates, registerFileSelectionCallback } from "./s2j/file_select";
import { ConfigKey, ElementID } from "./types";
import { UIModel,updateTracer, MappingInfo, updateGenerated } from "./generator";

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

interface LanguageConfig{
    box: HTMLElement,
    config: Map<string,InputListElement>
};

interface ColorMap {
    info :MappingInfo,
    color :string,
    sourceElement :HTMLElement|undefined,
    generatedElement :HTMLElement|undefined
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
        hover: {
            enabled: true,
        },
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


registerFileSelectionCallback(async() => {
    return await fetch("example/index.txt").then((res)=>res.text()).then((text)=>{
        text = text.replace(/\r\n/g,"\n");
        const c :FileCandidates = {
            placeHolder: "Select example file",
            candidates: text.split("\n").filter((e)=>e!==""),
        }
        return c;
    })
},(s)=>{
    console.log(s);
    if(s) {
        fetch(s).then((res)=>{
            if(!res.ok) {
                console.log(res);
                throw new Error(`failed to fetch ${s}`);
            }
            return res;
        }).then((res)=>res.text()).then((text)=>{
            editorUI.editor.executeEdits(editorUI.editor.getValue(),[{
                range: editorUI.editorModel.getFullModelRange(),
                text: text,
            }]);
        }).catch((e)=>{
            console.log(e);
        })
    }
})

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
    const curModel = editorUI.generated.getModel();
    if(curModel !== editorUI.generated_model) {
        curModel?.dispose();
    }
    editorUI.generated.setModel(model);
    editorUI.generated.setScrollTop(top);
}

const updateUI = async ()=>{
    const ui :UIModel = {
        getLanguageConfig: (lang,config) => {
            return commonUI.config.get(lang)?.config.get(config)?.value;
        },
        getValue: () => {
            return editorUI.editor.getValue();
        },
        setGenerated: setGenerated,
        setDefault: ()=> editorUI.setDefault(),
        mappingCode: mappingCode,
    };
    await updateGenerated(ui)
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
        if(updateTracer.editorAlreadyUpdated(origin))  {
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


window.addEventListener("keydown",async (e) => {
    if(e.ctrlKey&&e.key==="s"){
        e.preventDefault();
        updateUI();
    }
});


const onContentUpdate = async (e :monaco.editor.IModelContentChangedEvent)=>{
    e.changes.forEach((change)=>{
        console.log(change);
    });
    storage.setSourceCode(editorUI.editorModel.getValue());
    await updateUI();
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
    storage.setLangMode(mode as Language);
    commonUI.changeLanguageConfig(mode as Language);
    await updateUI();
}

const commonUI = {
    title_bar: title_bar,
    language_select: makeListBox(ElementID.LANGUAGE_SELECT,LanguageList,
        storage.getLangMode(),
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
        updateUI();
    }));
    const debugAST = new Map<string,InputListElement>();
    debugAST.set(ConfigKey.COMMON_FILE_NAME,fileName);
    commonUI.config.set(Language.JSON_DEBUG_AST,languageSpecificConfig(debugAST,ConfigKey.COMMON_FILE_NAME,(change) => {
        updateUI();
    }));
    const cppProto = new Map<string,InputListElement>();
    cppProto.set(ConfigKey.COMMON_FILE_NAME,fileName);
    commonUI.config.set(Language.CPP_PROTOTYPE,languageSpecificConfig(cppProto,ConfigKey.COMMON_FILE_NAME,(change) => {
        updateUI();
    }));
    const go = new Map<string,InputListElement>();
    go.set(ConfigKey.GO_USE_PUT,{
        "type": "checkbox",
        "value": false,
    });
    go.set(ConfigKey.COMMON_FILE_NAME,fileName);
    commonUI.config.set(Language.GO,languageSpecificConfig(go,ConfigKey.COMMON_FILE_NAME,(change) => {
        updateUI();
    }));
    const ast = new Map<string,InputListElement>();
    ast.set(ConfigKey.COMMON_FILE_NAME,fileName);
    commonUI.config.set(Language.JSON_AST,languageSpecificConfig(ast,ConfigKey.COMMON_FILE_NAME,(change) => {
        updateUI();
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
    cpp.set(ConfigKey.CPP_CHECK_OVERFLOW,{
        "type": "checkbox",
        "value": false,
    })
    cpp.set(ConfigKey.COMMON_FILE_NAME,fileName);
    commonUI.config.set(Language.CPP,languageSpecificConfig(cpp,ConfigKey.CPP_SOURCE_MAP,(change) => {
        updateTracer.getTraceID(); //dummy, consume id
        if(caches.recoloring !== null) {
            caches.recoloring();
        }
        updateUI();
    }));
}

commonUI.changeLanguageConfig(storage.getLangMode());
editorUI.editorModel.setValue(storage.getSourceCode());

document.getElementById(ElementID.BALL)?.remove();
document.getElementById(ElementID.BALL_BOUND)?.remove();

console.log(monaco.languages.getLanguages());
