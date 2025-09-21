import { clear } from "console";


const RAW_GIT_URL='https://raw.githubusercontent.com/on-keyday/utils/main/src/include'

const fetchRawGitContent = async (path :string,progress :(t:string)=>void) => {
    const url = `${RAW_GIT_URL}/${path}`;
    const timer = setTimeout(() => {
        progress(path);
    },100);
    let res = await fetch(url);
    if(!res.ok) {
        clearTimeout(timer);
        throw new Error(`failed to fetch ${url}`);
    }
    const source = await res.text();
    clearTimeout(timer);
    return source;
}

const resolveInclude = async (text :string,progress :(path :string)=>void,included:string[] = []):Promise<string> => {
    const include = /#include\s*<(.*\/.*)>/g;
    const matched = include.exec(text);
    if(!matched) return text;
    const path = matched[1];
    if(included.includes(path)) {
        console.log("duplicated include",path)
        const replaced = text.replace(matched[0],'');
        return await resolveInclude(replaced,progress,included);
    }
    const content = await fetchRawGitContent(path,progress);
    const removePragmaOnce = content.replace("#pragma once",'');
    const replaced = text.replace(matched[0], removePragmaOnce);
    return await resolveInclude(replaced,progress, [...included, path]);
}

export {resolveInclude,RAW_GIT_URL}
