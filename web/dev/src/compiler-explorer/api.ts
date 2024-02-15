import { Configuration } from "./types"

export const compileCpp = async (sourceCode :string) => {
    let config :Configuration = {
        source: sourceCode,
        options: {
            userArguments: "-O3 --std=c++20",
            compilerOptions: {
                skipAsm: false,
                executorRequest: false
            },
            filters: {
                binary: false,
                binaryObject: false,
                commentOnly: false,
                demangle: false,
                directives: false,
                execute: false,
                intel: false,
                labels: false,
                libraryCode: false,
                trim: false,
                debugCalls: false
            },
            tools: [],
            libraries: []
        },
        lang: "c++",
        allowStoreCodeDebug: false
    }
    console.log(config);
    const compiler = "clang1701";
    const compilerExplorerUrl = `https://godbolt.org/api/compiler/${compiler}/compile`;
    const response = await fetch(compilerExplorerUrl , {
        method: "POST",
        headers: {
            "Content-Type": "application/json",
            "Accept": "application/json"
        },
        body: JSON.stringify(config)
    });
    return await response.json();
}