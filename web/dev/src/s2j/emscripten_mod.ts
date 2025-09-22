/// <reference types="emscripten" />
export interface MyEmscriptenModule extends EmscriptenModule {
    ccall: typeof ccall;
    FS :typeof FS
    set_cancel_callback?: (f :(()=>boolean)|undefined) => void;
}

