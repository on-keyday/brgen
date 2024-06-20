
import * as monaco from  "../../node_modules/monaco-editor/esm/vs/editor/editor.api.js";
import '../../node_modules/monaco-editor/esm/vs/editor/contrib/semanticTokens/browser/documentSemanticTokens.js';
import '../../node_modules/monaco-editor/esm/vs/editor/contrib/semanticTokens/browser/viewportSemanticTokens.js';
import '../../node_modules/monaco-editor/esm/vs/editor/contrib/hover/browser/hoverContribution.js';
import * as caller from "./caller.js";
import {ast2ts,analyze} from "../../node_modules/ast2ts/index.js";
import { UpdateTracer } from "./update.js";
import { JobResult } from "./msg.js";
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
const context = {
    prevSemTokens : null as analyze.SemTokensStub|null,
    prevNode : null as ast2ts.Node|null,
    traceID : -1,
};

const provideTokens = async(model :monaco.editor.ITextModel, lastResultId :string|null, token: monaco.CancellationToken):Promise<monaco.languages.SemanticTokens|null> => {
    const traceID = updateTracer.getTraceID();
    context.traceID = traceID;
    const result = (await caller.getTokens(traceID,model.getValue(),{interpret_as_utf16:true})
    .catch(
        (e)=>{
            return e as JobResult;
        }
    ));
    if(updateTracer.editorAlreadyUpdated(result)){
        console.log("editor already updated for provideTokens");
        return null;
    }
    if(result.stdout === null||result.stdout===""){
        context.prevNode = null;
        monaco.editor.setModelMarkers(model,BRGEN_ID,[]);
        return null;
    }
    const tokens = JSON.parse(result.stdout!);
    if(!ast2ts.isTokenFile(tokens)||tokens.tokens === null){
        console.log("failed ",result);
        return null;
    }
    const sem = await analyze.analyzeSourceCode(null,tokens,async()=>{
        const cb = (result: JobResult)=>{
            if(result.stdout === null||result.stdout===""){
                context.prevNode = null;
                throw new Error(`failed to parse ast: ${result.stderr}`);
            }
            const ast = JSON.parse(result.stdout!);
            if(!ast2ts.isAstFile(ast)){
                console.log("failed ",result);
                throw new Error(`failed to parse ast: ${result.stderr}`);
            }
            if(ast.ast!==null) {
                const prog =ast2ts.parseAST(ast.ast);
                context.prevNode = prog;
                return {file:ast,ast:prog};
            }
            return {file:ast,ast:null};
        };
        return await caller.getAST(result.traceID,model.getValue(),{interpret_as_utf16:true}).
        then(cb,cb);
    },(pos)=>{
        const posStub = model.getPositionAt(pos);
        return {
            line: posStub.lineNumber-1,
            character: posStub.column-1,
        };
    },(diagnostics)=>{
        monaco.editor.setModelMarkers(model,BRGEN_ID,diagnostics.map((d)=>{return {
            message: d.message,
            severity: d?.severity === analyze.DiagnosticSeverityStub.Error ? monaco.MarkerSeverity.Error : monaco.MarkerSeverity.Warning,
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
    context.prevSemTokens = sem;
    return {
        data: new Uint32Array(sem.data),
    } as monaco.languages.SemanticTokens;
}

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
        provideDocumentSemanticTokens: provideTokens,
        releaseDocumentSemanticTokens: (resultId)=> {}
    }))
    disposeables.push(monaco.languages.registerHoverProvider(BRGEN_ID,{
        provideHover:async(model, position, token)=> {
            if(context.prevNode === null){
                return null;
            }
            const offset= model.getOffsetAt(position);
            const b=analyze.analyzeHover(context.prevNode,offset)
            if(b === null){
                return null;
            }
            const r: monaco.languages.Hover = {
                contents :[
                    {value: b.contents.value, isTrusted: true},
                ]
            };
            return r;
        },
    }));
})


