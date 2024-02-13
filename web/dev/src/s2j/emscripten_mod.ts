/// <reference types="emscripten" />
export interface MyEmscriptenModule extends EmscriptenModule {
    ccall: typeof ccall;
    FS :typeof FS
}

