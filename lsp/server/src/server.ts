/* --------------------------------------------------------------------------------------------
 * Copyright (c) Microsoft Corporation. All rights reserved.
 * Licensed under the MIT License. See License.txt in the project root for license information.
 * ------------------------------------------------------------------------------------------ */
import {
    createConnection,
    TextDocuments,
    ProposedFeatures,
    InitializeParams,
    DidChangeConfigurationNotification,
    TextDocumentSyncKind,
    InitializeResult,
    SemanticTokensLegend,
    SemanticTokensBuilder,
    SemanticTokenTypes,
    SemanticTokens,
    MarkupKind,
    HoverParams,
    DefinitionParams,
    Location,
    Diagnostic, 
    DiagnosticSeverity,
    Hover
} from 'vscode-languageserver/node';

import { Range, TextDocument } from 'vscode-languageserver-textdocument';

import {execFile} from "child_process";
import * as url from "url";
import { ast2ts } from "ast2ts";
import assert from 'node:assert/strict';
import { report } from 'process';






// Create a connection for the server, using Node's IPC as a transport.
// Also include all preview / proposed LSP features.
let connection = createConnection(ProposedFeatures.all);


// Create a simple text document manager.
let documents: TextDocuments<TextDocument> = new TextDocuments(TextDocument);

let hasConfigurationCapability: boolean = false;
let hasWorkspaceFolderCapability: boolean = false;

const legend:SemanticTokensLegend = {
    tokenTypes: [
        'comment',
        'keyword',
        'number',
        'operator',
        'string',
        'variable',
        'enumMember',
        'class',
        'function',
        'macro',
    ],
    tokenModifiers: [
        "declaration",
    ],
};

connection.onInitialize((params: InitializeParams) => {
    let capabilities = params.capabilities;

    // Does the client support the `workspace/configuration` request?
    // If not, we fall back using global settings.
    hasConfigurationCapability = !!(
        capabilities.workspace && !!capabilities.workspace.configuration
    );
    hasWorkspaceFolderCapability = !!(
        capabilities.workspace && !!capabilities.workspace.workspaceFolders
    );

    const result: InitializeResult = {
        capabilities: {
            textDocumentSync: TextDocumentSyncKind.Incremental,
            // Tell the client that this server supports code completion.
            completionProvider: {
                resolveProvider: true
            },
            semanticTokensProvider: {
                legend: legend,
                range: false,
                full: true,
            },
            hoverProvider: true,
            definitionProvider: true,
        }
    };
    if (hasWorkspaceFolderCapability) {
        result.capabilities.workspace = {
            workspaceFolders: {
                supported: true
            }
        };
    }
    return result;
});

connection.onInitialized(() => {
    if (hasConfigurationCapability) {
        // Register for all configuration changes.
        connection.client.register(DidChangeConfigurationNotification.type, undefined);
    }
    if (hasWorkspaceFolderCapability) {
        connection.workspace.onDidChangeWorkspaceFolders(_event => {
            connection.console.log('Workspace folder change event received.');
        });
    }
});

function execSrc2JSON<T>(exe_path :string,command :Array<string>,text :string,isT: (x :any) => x is T) {
    return new Promise<T>((resolve,reject)=>{
        const ch = execFile(exe_path,command ,(err,stdout,stderr)=>{
            if(err&&err?.code!==1){
                reject(err);
                return;
            }
            const tokens=JSON.parse(stdout);   
            console.log(`json: ${JSON.stringify(tokens)}`);
            if(stderr) {
                console.log(`stderr: ${stderr}`);
            }
            if(!isT(tokens)){
                reject(new TypeError("not valid file"))
                return;
            }
            resolve(tokens);
        });
        ch.stdin?.write(text);
        ch.stdin?.end();
        setTimeout(()=>{
            if(ch.exitCode===null){
                ch.kill();
                reject(new Error("timeout"));
            }
        },1000*10);
    });
}

const lexerCommand=(path :string) => ["--stdin","--stdin-name",path, "--lexer", "--no-color", "--print-on-error","--print-json","--interpret-mode","utf16","--detected-stdio-type"];
const parserCommand = (path :string) => ["--stdin","--stdin-name",path, "--no-color", "--print-on-error","--print-json","--interpret-mode","utf16","--detected-stdio-type"];


class DocumentInfo {
    readonly uri :string;
    prevSemanticTokens :SemanticTokens | null = null;
    prevFile :ast2ts.AstFile| null = null;
    prevNode :ast2ts.Node | null = null;
    prevText :string | null = null;

    constructor(uri :string){
        this.uri = uri;
    }
}

let documentInfos = new Map<string,DocumentInfo>();

const stringEscape = (s :string) => {
    return s.replace(/\\/g,"\\\\").replace(/"/g,"\\\"").replace(/\n/g,"\\n").replace(/\r/g,"\\r");
}


const reportDiagnostic = (doc :TextDocument,errs :ast2ts.SrcError) => {
    const diagnostics = errs.errs.map((err)=> {
        if(err.loc.file != 1) {
            return null; // skip other files
        }
        const b = doc.positionAt(err.loc.pos.begin);
        const e = doc.positionAt(err.loc.pos.end);
        const range :Range = {
            start: b,
            end: e,
        };
        return {
            severity: err.warn ? DiagnosticSeverity.Warning : DiagnosticSeverity.Error,
            range: range,
            message: err.msg,
            source: 'brgen-lsp',
        } as Diagnostic;
    }).filter((x)=>x!==null) as Diagnostic[];
    console.log(`diagnostics: ${JSON.stringify(diagnostics.map((x)=>x.message))}`);
    connection.sendDiagnostics({ uri: doc.uri, diagnostics });
    console.log("diagnostics sent");
}

const tokenizeSourceImpl  = async (doc :TextDocument,docInfo :DocumentInfo) =>{
    const path = url.fileURLToPath(doc.uri)
    const text = doc.getText();
    console.log(`URI: ${doc.uri}`);
    console.time("semanticColoring")
    console.time("load settings")
    const settings = await getDocumentSettings(doc.uri);
    console.timeEnd("load settings")
    console.log(`settings: ${JSON.stringify(settings)}`);
    console.time("tokenize")
    let tokens_ :ast2ts.TokenFile;
    try {
        tokens_ =  await execSrc2JSON(settings.src2json,lexerCommand(path),text,ast2ts.isTokenFile);
    } catch(e :any) {
        console.timeEnd("tokenize")
        console.timeEnd("semanticColoring")
        throw e;
    }
    const tokens = tokens_;
    console.timeEnd("tokenize")
    const mapForTokenTypes = new Map<ast2ts.TokenTag,SemanticTokenTypes>([
        [ast2ts.TokenTag.comment,SemanticTokenTypes.comment],
        [ast2ts.TokenTag.keyword,SemanticTokenTypes.keyword],
        [ast2ts.TokenTag.bool_literal,SemanticTokenTypes.macro],
        [ast2ts.TokenTag.int_literal,SemanticTokenTypes.number],
        [ast2ts.TokenTag.str_literal,SemanticTokenTypes.string],
        [ast2ts.TokenTag.ident,SemanticTokenTypes.variable],
        [ast2ts.TokenTag.punct,SemanticTokenTypes.operator],
    ]);
    const mapTokenTypesIndex = new Map<SemanticTokenTypes,number>([
        [SemanticTokenTypes.comment,0],
        [SemanticTokenTypes.keyword,1],
        [SemanticTokenTypes.number,2],
        [SemanticTokenTypes.operator,3],
        [SemanticTokenTypes.string,4], 
        [SemanticTokenTypes.variable,5],
        [SemanticTokenTypes.enumMember,6],
        [SemanticTokenTypes.class,7],
        [SemanticTokenTypes.function,8],
        [SemanticTokenTypes.macro,9],
    ]); 
    assert(tokens.tokens);
    type L = { readonly loc :ast2ts.Loc, readonly length :number, readonly index :number}
    const locList = new Array<L>();
    tokens.tokens.forEach((token:ast2ts.Token)=>{
        console.log(`token: ${stringEscape(token.token)} ${token.tag} ${token.loc.line} ${token.loc.col}`)
        if(token.tag===ast2ts.TokenTag.keyword){
            if(token.token==="input"||token.token==="output"||token.token=="config"||
               token.token=="fn"||token.token=="format"||token.token=="enum"||
               token.token=="cast"||token.token=="state"){
                locList.push({loc:token.loc,length: token.token.length,index:9});
                return;
            }
        }
        const type = mapForTokenTypes.get(token.tag);
        if(type===undefined){
            return;
        }
        const index = mapTokenTypesIndex.get(type);
        if(index===undefined){
            return;
        }
        locList.push({loc:token.loc,length: token.token.length,index:index});
    });
    const generateSemanticTokens = () => {
        const builder = new SemanticTokensBuilder();
        const removeList = new Array<L>();
        locList.sort((a,b)=>{
            if(a.loc.line == b.loc.line&&
                a.loc.col == b.loc.col){
                if(a.index == 2){
                    removeList.push(b);
                }
                else if(b.index == 2){
                    removeList.push(a);
                }
                else if(a.index < b.index) {
                    removeList.push(a);
                }
                else{
                    removeList.push(b);
                }
            }
            if(a.loc.line<b.loc.line){
                return -1;
            }
            if(a.loc.line>b.loc.line){
                return 1;
            }
            if(a.loc.col<b.loc.col){
                return -1;
            }
            if(a.loc.col>b.loc.col){
                return 1;
            }
            return 0;
        });
        locList.filter((x)=>!removeList.includes(x)).forEach((x)=>{
            builder.push(x.loc.line-1,x.loc.col-1,x.length,x.index,0);
        });
        const semanticTokens = builder.build();
        console.log(`semanticTokens (parsed): ${JSON.stringify(semanticTokens)}`);
        console.timeEnd("semanticColoring")
        docInfo.prevSemanticTokens = semanticTokens;
        return semanticTokens;
    };
    let ast_ :ast2ts.AstFile;
    try {
        console.time("parse")
        ast_ = await execSrc2JSON(settings.src2json,parserCommand(path),text,ast2ts.isAstFile);
    } catch(e :any) {
        console.timeEnd("parse")
        console.log(`error: ${e}`);
        return generateSemanticTokens();
    }
    console.timeEnd("parse")
    const ast = ast_;
    if(ast.error !== null) {
        reportDiagnostic(doc,ast.error);
    }
    else {
        reportDiagnostic(doc,{errs:[]});
    }
    if(ast.ast === null) {
        throw new Error("ast is null, use previous ast");
    }
    const prog = ast2ts.parseAST(ast.ast);
    ast2ts.walk(prog,(f,node)=>{
        if(node.loc.file!=1) {
            console.log("prevent file boundary: "+node.loc.file)
            return; // skip other files
        }
        ast2ts.walk(node,f);
        if(ast2ts.isIdent(node)){
            const line = node.loc.line-1;
            const col = node.loc.col-1;
            assert(line>=0,`line: ${line}`);
            assert(col>=0,`col: ${col}`);
            assert(node.ident.length>0,`ident: ${node.ident}`)
            let n = node;
            console.log(`ident: ${n.ident} ${n.usage}`)
            let counter = 0;
            for(;;counter++){
                switch(n.usage) {
                    case ast2ts.IdentUsage.unknown:
                        break;
                    case ast2ts.IdentUsage.reference:
                        if(ast2ts.isIdent(n.base)){
                            if(counter> 100) {
                                console.log(n,n.base)
                                throw new Error("what happened?");
                            }
                            console.log("-> "+n.base.usage);
                            n = n.base;
                            continue;
                        }
                        break;
                    case ast2ts.IdentUsage.reference_member:
                        if(ast2ts.isMemberAccess(n.base)){
                            if(ast2ts.isIdent(n.base.base)){
                                if(counter> 100) {
                                    console.log(n,n.base)
                                    throw new Error("what happened?");
                                }
                                console.log("-> "+n.base.base.usage);
                                n = n.base.base;
                                continue;
                            }
                        }
                        break;
                    case ast2ts.IdentUsage.define_variable:
                        break;
                    case ast2ts.IdentUsage.define_field:   
                    case ast2ts.IdentUsage.define_const:
                    case ast2ts.IdentUsage.define_enum_member:
                    case ast2ts.IdentUsage.define_arg:
                        locList.push({loc: node.loc,length: node.ident.length,index:6});
                        break;   
                    case ast2ts.IdentUsage.define_format:
                    case ast2ts.IdentUsage.define_enum:
                    case ast2ts.IdentUsage.define_state:
                    case ast2ts.IdentUsage.reference_type:
                    case ast2ts.IdentUsage.define_cast_fn:
                    case ast2ts.IdentUsage.maybe_type:
                        locList.push({loc: node.loc,length: node.ident.length,index:7});
                        break;
                    case ast2ts.IdentUsage.define_fn:
                        locList.push({loc: node.loc,length: node.ident.length,index:8});
                        break;
                    case ast2ts.IdentUsage.reference_builtin_fn:
                        locList.push({loc: node.loc,length: node.ident.length,index:9});
                        break;
                }
                break;
            }
        }
        else if(ast2ts.isIntType(node)&&node.is_explicit){
            locList.push({loc: node.loc,length: node.loc.pos.end - node.loc.pos.begin,index:7});
        }
        else if(ast2ts.isVoidType(node)&&node.is_explicit){
            locList.push({loc: node.loc,length: node.loc.pos.end - node.loc.pos.begin,index:7});
        }
        else if(ast2ts.isBoolType(node)&&node.is_explicit){
            locList.push({loc: node.loc,length: node.loc.pos.end - node.loc.pos.begin,index:7});
        }
        else if(ast2ts.isFloatType(node)&&node.is_explicit){
            locList.push({loc: node.loc,length: node.loc.pos.end - node.loc.pos.begin,index:7});
        }
        return true
    });
    docInfo.prevFile = ast;
    docInfo.prevNode = prog;
    return generateSemanticTokens();
};

const getOrCreateDocumentInfo = (doc :TextDocument) => {
    const found = documentInfos.get(doc.uri);
    if(found===undefined){
        const info = new DocumentInfo(doc.uri);
        documentInfos.set(doc.uri,info);
        return info;
    }
    return found;
}

const tokenizeSource = async (doc :TextDocument) =>{
    const documentInfo = getOrCreateDocumentInfo(doc);
    try {
        return await tokenizeSourceImpl(doc,documentInfo);
    }catch(e :any) {
        console.log(`error: ${e}`);
        return documentInfo.prevSemanticTokens;
    }
}

connection.onRequest("textDocument/semanticTokens/full",async (params)=>{
    console.log(`textDocument/semanticTokens/full: ${JSON.stringify(params)}`);
    const doc = documents.get(params?.textDocument?.uri);
    if(doc===undefined){
        console.log(`document ${params?.textDocument?.uri} is not found`);
        return null; 
    }
    return await tokenizeSource(doc);
});


const hover = async (params :HoverParams)=>{
    console.log(`textDocument/hover: ${JSON.stringify(params)}`);
    const hover = documents.get(params?.textDocument?.uri);
    if(hover===undefined){
        return null;
    }
    hover.getText();
    const pos =  hover.offsetAt(params.position);
    console.log("target pos: %d",pos)
    const docInfo = getOrCreateDocumentInfo(hover);
    if(docInfo.prevNode===null) {
        return null;
    }
    let found :any;
    ast2ts.walk(docInfo.prevNode,(f,node)=>{
        if(found!==undefined){
            return false;
        }
        if(node.loc.file!=1) {
            console.log("prevent file boundary: "+node.loc.file)
            return; // skip other files
        }
        if(node.loc.pos.begin<=pos&&pos<=node.loc.pos.end){
            if(ast2ts.isIdent(node)){
                console.log(`found: ${node.ident} ${JSON.stringify(node.loc)}`)
                found = node;
                return;
            }
            else if(ast2ts.isAssert(node)) {
                console.log(`found: ${node.node_type} ${JSON.stringify(node.loc)}`)
                found = node;
                return;
            }
            console.log(`hit: ${node.node_type} ${JSON.stringify(node.loc)}`)
        }
        ast2ts.walk(node,f);
        if(ast2ts.isMember(node)){
            console.log("walked: "+node.node_type);
            if(node.ident!=null){
                console.log("ident: "+node.ident.ident+" "+JSON.stringify(node.ident.loc));
            }
        }
        else if(ast2ts.isIdent(node)){
            console.log("walked: "+node.node_type);
            console.log("ident: "+node.ident+" "+JSON.stringify(node.loc));
        }
        else{
            console.log("walked: "+node.node_type)
        }
    });
    const makeHover = (ident :string,role :string) => {
        return {
            contents: {
                kind: MarkupKind.Markdown,
                value: `### ${ident}\n\n${role}`,
            },
        } as Hover
    }
    if(found === null) {
        return null;
    }

    const bitSize = (bit :number|null|undefined) => {
        if(bit===undefined){
            return "unknown";
        }
        if(bit===null){
            return "dynamic";
        }
        if(bit%8===0){
            return `${bit/8} byte`;
        }
        return `${bit} bit`;
    }
    const unwrapType = (type :ast2ts.Type|null|undefined) :string => {
        if(!type){
            return "unknown";
        }
        if(ast2ts.isIdentType(type)){
           return unwrapType(type.base); 
        }
        return type.node_type;
    }

    if(ast2ts.isIdent(found)){
        let ident = found as ast2ts.Ident;
        for(;;){
            switch (ident.usage) {
                case ast2ts.IdentUsage.unknown:
                    return makeHover(ident.ident,"unknown identifier");
                case ast2ts.IdentUsage.reference:
                    if(ast2ts.isIdent(ident.base)){
                        ident = ident.base;
                        continue;
                    }
                    return makeHover(ident.ident,"unspecified reference");
                case ast2ts.IdentUsage.reference_member:
                    if(ast2ts.isMemberAccess(ident.base)){
                        if(ast2ts.isIdent(ident.base.base)){
                            ident = ident.base.base;
                            continue;
                        }
                    }
                    return makeHover(ident.ident,"unspecified member reference");
                case ast2ts.IdentUsage.define_variable:
                    return makeHover(ident.ident,`variable (type: ${unwrapType(ident.expr_type)}, size: ${bitSize(ident.expr_type?.bit_size)} constant level: ${ident.constant_level})`);
                case ast2ts.IdentUsage.define_const:
                    return makeHover(ident.ident,`constant (type: ${unwrapType(ident.expr_type)}, size: ${bitSize(ident.expr_type?.bit_size)} constant level: ${ident.constant_level})`);
                case ast2ts.IdentUsage.define_field:
                    if(ast2ts.isField(ident.base)){
                        return makeHover(ident.ident, 
                            `
+ field 
    + type: ${unwrapType(ident.base.field_type)}
    + size: ${bitSize(ident.base.field_type?.bit_size)}
    + offset(from begin): ${bitSize(ident.base.offset_bit)}
    + offset(from recent fixed): ${bitSize(ident.base.offset_recent)}
    + offset(from end): ${bitSize(ident.base.tail_offset_bit)}
    + offset(from recent fixed): ${bitSize(ident.base.tail_offset_recent)}
    + align: ${ident.base.bit_alignment}, 
    + type align: ${ident.base.field_type?.bit_alignment||"unknown"}
    + follow: ${ident.base.follow}
    + eventual_follow: ${ident.base.eventual_follow}`);
                    }
                    return makeHover(ident.ident,"field");
                case ast2ts.IdentUsage.define_enum_member:
                    if(ast2ts.isEnumType(ident.expr_type)) {
                        return makeHover(ident.ident,`enum member (type: ${ident.expr_type.base?.ident?.ident||"unknown"}, size: ${bitSize(ident.expr_type?.bit_size)})`);
                    }
                    return makeHover(ident.ident,"enum member");
                case ast2ts.IdentUsage.define_format:
                    if(ast2ts.isFormat(ident.base)){
                        return makeHover(ident.ident,
                        `
+ format 
    + size: ${bitSize(ident.base.body?.struct_type?.bit_size)}
    + fixed header size: ${bitSize(ident.base.body?.struct_type?.fixed_header_size)}
    + fixed tail size: ${bitSize(ident.base.body?.struct_type?.fixed_tail_size)}
    + algin: ${ident.base.body?.struct_type?.bit_alignment||"unknown"}
    ${ident.base.body?.struct_type?.non_dynamic?"+ non_dynamic\n    ":""}${ident.base.body?.struct_type?.recursive?"+ recursive\n":""}
`);
                    }
                    return makeHover(ident.ident,"format"); 
                case ast2ts.IdentUsage.define_enum:
                    if(ast2ts.isEnum(ident.base)){
                        return makeHover(ident.ident,`enum (size: ${bitSize(ident.base.base_type?.bit_size)}, align: ${ident.base?.base_type?.bit_alignment || "unknown"})`);
                    }
                    return makeHover(ident.ident,"enum");
                case ast2ts.IdentUsage.define_state:
                    return makeHover(ident.ident,"state");
                case ast2ts.IdentUsage.reference_type:
                    if(ast2ts.isIdent(ident.base)){
                        ident = ident.base;
                        continue;
                    }
                    return makeHover(ident.ident,"type");
                case ast2ts.IdentUsage.define_cast_fn:
                    return makeHover(ident.ident,"cast function");
                case ast2ts.IdentUsage.maybe_type:
                    return makeHover(ident.ident,"maybe type");
                case ast2ts.IdentUsage.define_fn:
                    return makeHover(ident.ident,"function");
                case ast2ts.IdentUsage.define_arg:
                    return makeHover(ident.ident,"argument");
                case ast2ts.IdentUsage.reference_builtin_fn:
                    return makeHover(ident.ident,"builtin function");
                default:
                    return makeHover(ident.ident,"unknown identifier");
            }
        }     
    }
    else if(ast2ts.isAssert(found)){
        return makeHover("assert",`assertion ${found.is_io_related?"(io_related)":""}`); 
    }
    else if(ast2ts.isTypeLiteral(found)){
        return makeHover("type literal",`type literal (type: ${found.type_literal?.node_type || "unknown"}, size: ${bitSize(found.type_literal?.bit_size)})`);
    }
}

connection.onHover(hover)

const definitionHandler = async (params :DefinitionParams) => {
    console.log(`textDocument/definition: ${JSON.stringify(params)}`);
    const doc = documents.get(params?.textDocument?.uri);
    if(doc===undefined){
        console.log(`document ${params?.textDocument?.uri} is not found`);
        return null; 
    }
    const pos =  doc.offsetAt(params.position);
    const docInfo = getOrCreateDocumentInfo(doc);
    if(docInfo.prevNode===null) {
        return null;
    }
    let found :any;
    ast2ts.walk(docInfo.prevNode,(f,node)=>{
        if(node.loc.file!=1) {
            console.log("prevent file boundary: "+node.loc.file)
            return; // skip other files
        }
        if(node.loc.pos.begin<=pos&&pos<=node.loc.pos.end){
            if(ast2ts.isIdent(node)){
                console.log(`found: ${node.ident} ${JSON.stringify(node.loc)}`)
                found = node;
                return false; // skip children
            }
            console.log(`hit: ${node.node_type} ${JSON.stringify(node.loc)}`)
        }
        ast2ts.walk(node,f);
    });
    const fileToLink = (loc :ast2ts.Loc, file :ast2ts.AstFile) => {
        const path = file.files[loc.file-1];
        const range :Range = {
            start: {line: loc.line-1, character: loc.col-1},
            end: {line: loc.line-1, character: loc.col-1},
        };
        const location :Location = {
            uri: url.pathToFileURL(path).toString(),
            range: range,
        };
        console.log(location);
        return location;
    }
    if(ast2ts.isIdent(found)){
        let ident = found;
        for(;;){
            switch (ident.usage) {
                case ast2ts.IdentUsage.unknown:
                    return null;
                case ast2ts.IdentUsage.reference:
                    if(ast2ts.isIdent(ident.base)){
                        ident = ident.base;
                        continue;
                    }
                    return null;
                case ast2ts.IdentUsage.reference_type:
                    if(ast2ts.isIdent(ident.base)){
                        ident = ident.base;
                        continue;
                    }
                    return null;
                case ast2ts.IdentUsage.reference_member:
                    if(ast2ts.isMemberAccess(ident.base)){
                        if(ast2ts.isIdent(ident.base.base)){
                            ident = ident.base.base;
                            continue;
                        }
                    }
                    return null;
                case ast2ts.IdentUsage.define_variable:
                case ast2ts.IdentUsage.define_field:
                case ast2ts.IdentUsage.define_const:
                case ast2ts.IdentUsage.define_enum_member:
                case ast2ts.IdentUsage.define_format:
                case ast2ts.IdentUsage.define_enum:
                case ast2ts.IdentUsage.define_cast_fn:
                case ast2ts.IdentUsage.maybe_type:
                case ast2ts.IdentUsage.define_fn:
                case ast2ts.IdentUsage.define_state:
                case ast2ts.IdentUsage.define_arg:
                    return fileToLink(ident.loc,docInfo.prevFile!);
                default:
                    return null;
            }
        }
    }
    return null; 
}

connection.onDefinition(definitionHandler);

// The example settings
interface BrgenLSPSettings {
    src2json :string;
}

// The global settings, used when the `workspace/configuration` request is not supported by the client.
// Please note that this is not the case when using this server with the client provided in this example
// but could happen with other clients.
const defaultSettings: BrgenLSPSettings = { src2json: `./tool/src2json${(process.platform === "win32" ? ".exe" : "")}` };
let globalSettings: BrgenLSPSettings = defaultSettings;

// Cache the settings of all open documents
let documentSettings: Map<string, Thenable<BrgenLSPSettings>> = new Map();

connection.onDidChangeConfiguration(change => {
    if (hasConfigurationCapability) {
        // Reset all cached document settings
        documentSettings.clear();
    } else {
        globalSettings = <BrgenLSPSettings>(
            (change.settings.brgenLsp || defaultSettings)
        );
    }

    // Revalidate all open text documents
    // documents.all().forEach(validateTextDocument);
});

const  getDocumentSettings = async(resource: string) => {
    if (!hasConfigurationCapability) {
        return Promise.resolve(globalSettings);
    }
    let result = documentSettings.get(resource);
    if (result===undefined) {
        result = connection.workspace.getConfiguration({
            scopeUri: resource,
            section: 'brgen-lsp'
        });
        documentSettings.set(resource, result);
    }
    return result.then((x)=>{
        if(x===null){
            console.log("using default settings")
            return defaultSettings;
        }
        if(x.src2json === undefined || x.src2json === null || x.src2json === ""){
            console.log("using default settings")
            return defaultSettings;
        }
        return x;
    },(_)=>{
        console.log("use default settings")
        return defaultSettings;
    });
}

// Only keep settings for open documents
documents.onDidClose(e => {
    documentSettings.delete(e.document.uri);
    documentInfos.delete(e.document.uri);
});

connection.onCompletion(async (textDocumentPosition) => {
    return null;
});


// Make the text document manager listen on the connection
// for open, change and close text document events
documents.listen(connection);

// Listen on the connection
connection.listen();


