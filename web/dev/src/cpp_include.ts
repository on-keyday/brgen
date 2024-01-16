

const RAW_GIT_URL='https://raw.githubusercontent.com/on-keyday/utils/main/src/include'

const fetchRawGitContent = async (url :string) => {
    url = `${RAW_GIT_URL}/${url}`;
    const res = await fetch(url);
    if(!res.ok) throw new Error('fetch failed');
    return await res.text();
}

const resolveInclude = async (text :string,included:string[] = []):Promise<string> => {
    const include = /#include\s*<(.*\/.*)>/g;
    const matched = include.exec(text);
    if(!matched) return text;
    const path = matched[1];
    if(included.includes(path)) {
        console.log("duplicated include",path)
        const replaced = text.replace(matched[0],'');
        return await resolveInclude(replaced,included);
    }
    const content = await fetchRawGitContent(path);
    const replaced = text.replace(matched[0], content);
    return await resolveInclude(replaced, [...included, path]);
}

export {resolveInclude,RAW_GIT_URL}
