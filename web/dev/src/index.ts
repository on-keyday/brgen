
/// <reference types="emscripten" />


/// <reference path="../lib/src2json.js"/>

declare var Module :EmscriptenModuleFactory

export {}

const textCapture = {
    stdout : "",
    stderr : ""
}

let mod = await Module({
    locateFile: (file) => {
        return `../lib/${file}`
    },
    print: (text) => {
        console.log(text)
        textCapture.stdout += text + "\n"
    },
    printErr: (text) => {
        console.error(text)
        textCapture.stderr += text + "\n"
    }
})


mod.arguments = [""]