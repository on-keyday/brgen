// HACK(on-keyday): this is stub for the input service that is not exported to typescript
//                  compatibility for the future is not guaranteed

import {EditorAction} from "monaco-editor/esm/vs/editor/browser/editorExtensions.js"
import {IQuickInputService} from "monaco-editor/esm/vs/platform/quickinput/common/quickInput.js"
import {registerEditorAction} from "monaco-editor/esm/vs/editor/browser/editorExtensions.js"

const cacheRef = {
    fileCandidates: () => {
        return Promise.resolve( {
            placeHolder: 'Select a file',
            candidates: ['dummy']
        })
    },
    fileSelected: (pick) => {
        console.log(pick)
    }
};

class FileSelectionProvider extends EditorAction {
    constructor() {
        super({
            id: "brgen.loadExampleFile",
            label: 'Load Example File',
            alias: 'Load Example File',
            precondition: undefined,
            contextMenuOpts: {
                group: 'z_commands',
                order: 1
            }
        })
    }

    run(accessor,editor) {
        const quickInputService = accessor.get(IQuickInputService);
        const r= cacheRef.fileCandidates();
        r.then((s)=>{
            const picks = s.candidates.map((v)=>{
                return {
                    id: v,
                    label: v,
                }
            })
            quickInputService.pick(picks,{ placeHolder: s.placeHolder, activeItem: picks[0] })
            .then((pick) =>{
                cacheRef.fileSelected(pick)
            })
        })
    }
}

registerEditorAction(FileSelectionProvider)
export {cacheRef}
