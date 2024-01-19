
import * as monaco from  "../../node_modules/monaco-editor/esm/vs/editor/editor.api.js";
import '../../node_modules/monaco-editor/esm/vs/editor/contrib/semanticTokens/browser/documentSemanticTokens.js';
import '../../node_modules/monaco-editor/esm/vs/editor/contrib/semanticTokens/browser/viewportSemanticTokens.js';
import * as caller from "./caller.js";
import {ast2ts} from "../../node_modules/ast2ts/index.js";

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
		{ token: "comment", foreground: "aaaaaa", fontStyle: "italic" },
		{ token: "operator", foreground: "000000" },
		{ token: "keyword", foreground: "0000ff", fontStyle: "bold" },
		{ token: "variable", foreground: "3e5bbf" },
	],
    
});

monaco.languages.onLanguage(BRGEN_ID,()=>{
    disposeables.forEach((d)=>d.dispose());
    disposeables = [];
    disposeables.push(monaco.languages.registerDocumentSemanticTokensProvider(BRGEN_ID,{
        getLegend: ()=> {
            return {
                tokenTypes: ['keyword','variable','string','number','operator','comment'],
                tokenModifiers: ['declaration','definition','readonly','static','async','abstract','deprecated','modification','documentation'],
            } as monaco.languages.SemanticTokensLegend;
        },
        provideDocumentSemanticTokens: async(model, lastResultId, token)=> {
            const result = await caller.getTokens(model.getValue());
            if(result.code !== 0){
                console.log("failed ",result);
                return null;
            }
            const tokens = JSON.parse(result.stdout!);
            if(!ast2ts.isTokenFile(tokens)||tokens.tokens === null){
                console.log("failed ",result);
                return null;
            }
            const tagMap = new Map<string,number>();
            tagMap.set(ast2ts.TokenTag.keyword,0);
            tagMap.set(ast2ts.TokenTag.ident,1);
            tagMap.set(ast2ts.TokenTag.str_literal,2);
            tagMap.set(ast2ts.TokenTag.int_literal,3);
            tagMap.set(ast2ts.TokenTag.punct,3);
            tagMap.set(ast2ts.TokenTag.comment,4);
            let prevLine = 1;      
            let prevChar = 1;   
            const data : number[] = [];   
            tokens.tokens.forEach((t: ast2ts.Token)=>{
                
                const tokenType = tagMap.get(t.tag);
                if(tokenType === undefined){
                    return;
                }
                const startAt = model.getPositionAt(t.loc.pos.begin);
                const endAt = model.getPositionAt(t.loc.pos.end);
                const line = startAt.lineNumber;
                const char = startAt.column;
                const length = endAt.column - startAt.column;
                data.push(line - prevLine,prevLine == line ? char - prevChar : char, length, tokenType,0);
                prevLine = line;
                prevChar = char;
            })
            return {
                data: new Uint32Array(data),
            } as monaco.languages.SemanticTokens;
        },
        releaseDocumentSemanticTokens: (resultId)=> {}
    }))
})




