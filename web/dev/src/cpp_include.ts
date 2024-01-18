import { clear } from "console";


const RAW_GIT_URL='https://raw.githubusercontent.com/on-keyday/utils/main/src/include'

const fetchRawGitContent = async (url :string,progress :(t:string)=>void) => {
    url = `${RAW_GIT_URL}/${url}`;
    const timer = setTimeout(() => {
        progress(url);
    },1000);
    let res = await fetch(url,{cache: 'only-if-cached'});
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
