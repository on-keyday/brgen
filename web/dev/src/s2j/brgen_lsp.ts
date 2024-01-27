
import * as monaco from  "../../node_modules/monaco-editor/esm/vs/editor/editor.api.js";
import '../../node_modules/monaco-editor/esm/vs/editor/contrib/semanticTokens/browser/documentSemanticTokens.js';
import '../../node_modules/monaco-editor/esm/vs/editor/contrib/semanticTokens/browser/viewportSemanticTokens.js';
import * as caller from "./caller.js";
import {ast2ts,analyze} from "../../node_modules/ast2ts/index.js";
import { UpdateTracer } from "./update.js";
import { DiagnosticSeverity, MarkupKind } from "vscode-languageserver";

const BRGEN_ID = 'brgen'

monaco.languages.register({
    id: BRGEN_ID
})
let disposeables : monaco.IDisposable[] = [];

// add some missing tokens
monaco.editor.defineTheme("brgen-theme", {
	base: "vs",
	inherit: true,
	colors: {},
	rules: [
		{ token: "comment", foreground: "#aaaaaa", fontStyle: "italic" },
		{ token: "operator", foreground: "#000000" },
		{ token: "keyword", foreground: "#0000ff", fontStyle: "bold" },
		{ token: "variable", foreground: "#3e5bbf" },
        {token: "macro", foreground: "#0000ff", fontStyle: "bold"},
        {token: "class", foreground: "#619E92", fontStyle: "bold"},
        {token: "enumMember", foreground: "#20a6f9", fontStyle: "bold"},
	],

});

const updateTracer = new UpdateTracer();

monaco.languages.onLanguage(BRGEN_ID,()=>{
    disposeables.forEach((d)=>d.dispose());
    disposeables = [];
    disposeables.push(monaco.languages.registerDocumentSemanticTokensProvider(BRGEN_ID,{
        getLegend: ()=> {
            return {
                tokenTypes: analyze.legendMapping,
                tokenModifiers: ['declaration','definition','readonly','static','async','abstract','deprecated','modification','documentation'],
            } as monaco.languages.SemanticTokensLegend;
        },
        provideDocumentSemanticTokens: async(model, lastResultId, token)=> {
            const traceID = updateTracer.getTraceID();
            const result = await caller.getTokens(traceID,model.getValue(),{interpret_as_utf16:true});
            if(updateTracer.editorAlreadyUpdated(result)){
                return null;
            }
            if(result.code !== 0){
                console.log("failed ",result);
                return null;
            }
            const tokens = JSON.parse(result.stdout!);
            if(!ast2ts.isTokenFile(tokens)||tokens.tokens === null){
                console.log("failed ",result);
                return null;
            }
            const sem = await analyze.analyzeSourceCode(tokens,async()=>{
                return await caller.getAST(result.traceID,model.getValue(),{interpret_as_utf16:true}).
                then((result)=>{
                    if(result.code !== 0){
                        console.log("failed ",result);
                        throw new Error("failed to parse ast");
                    }
                    const ast = JSON.parse(result.stdout!);
                    if(!ast2ts.isAstFile(ast)){
                        console.log("failed ",result);
                        throw new Error("failed to parse ast");
                    }
                    return ast;
                });
            },(pos)=>model.getPositionAt(pos),(diagnostics)=>{
                monaco.editor.setModelMarkers(model,BRGEN_ID,diagnostics.map((d)=>{return {
                    message: d.message,
                    severity: d?.severity === DiagnosticSeverity.Error ? monaco.MarkerSeverity.Error : monaco.MarkerSeverity.Warning,
                    startLineNumber: d.range.start.line+1,
                    startColumn: d.range.start.character+1,
                    endLineNumber: d.range.end.line+1,
                    endColumn: d.range.end.character+1,
                }}));
            });
            if(sem === null){
                console.log("failed to analyze");
                return null;
            }
            console.log("token: ",sem.data);
            return {
                data: new Uint32Array(sem.data),
            } as monaco.languages.SemanticTokens;
        },
        releaseDocumentSemanticTokens: (resultId)=> {}
    }))
})




