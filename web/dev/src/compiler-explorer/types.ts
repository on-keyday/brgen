export interface CompilerOptions {
    skipAsm: boolean;
    executorRequest: boolean;
}

export interface Filters {
    binary: boolean;
    binaryObject: boolean;
    commentOnly: boolean;
    demangle: boolean;
    directives: boolean;
    execute: boolean;
    intel: boolean;
    labels: boolean;
    libraryCode: boolean;
    trim: boolean;
    debugCalls: boolean;
}

export interface Tool {
    id: string;
    args: string;
}

export interface Library {
    id: string;
    version: string;
}

export  interface Options {
    userArguments: string;
    compilerOptions: CompilerOptions;
    filters: Filters;
    tools: Tool[];
    libraries: Library[];
}

export interface Configuration {
    source: string;
    options: Options;
    lang?: string;
    allowStoreCodeDebug: boolean;
}
