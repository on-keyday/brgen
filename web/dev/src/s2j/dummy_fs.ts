import { MyEmscriptenModule } from "./emscripten_mod"

export const fetchImportFile = async(mod :MyEmscriptenModule,sourceCode :string) => {
    const regex = /config\s*\.\s*import\s*\(.*\"(.*)\".*\)/g
    const matched = sourceCode.matchAll(regex);
    for(const match of matched) {
        const path = match[1];
        const check = decodeURIComponent(path);
        if(!check.endsWith(".bgn")|| check.includes("..")){
            continue;
        }
        const p = await fetch("../example/"+path);
        if(!p.ok){
            continue; // ignore non exist file
        }
        const text = await p.text();
        console.log("write",path);
        mod.FS.writeFile(path,text);
    }   
    const actualOpen = mod.FS.open 
    mod.FS.open = (path :string,...args) => {
        console.log("open",path);
        const got = actualOpen(path,...args);
        console.log("got",got);
        return got;
    }
    const actualMmap = mod.FS.mmap 
    mod.FS.mmap = (...args) => {
        console.log("mmap",args);
        const got = actualMmap(...args);
        console.log("got",got);
        return got;
    }
    const actualStat = mod.FS.stat
    mod.FS.stat = (path :string) => {
        console.log("stat",path);
        const got = actualStat(path);
        console.log("got",got);
        return got;
    }
}
