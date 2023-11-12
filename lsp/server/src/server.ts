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
    TypeDefinitionParams,
    Location,
} from 'vscode-languageserver/node';

import { Range, TextDocument } from 'vscode-languageserver-textdocument';

import {execFile} from "child_process";
import * as url from "url";
import { ast2ts } from "ast2ts";
import assert from 'node:assert/strict';





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
            typeDefinitionProvider: true,
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
            if(err){
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
            if(ch.exitCode!==null){
                ch.kill();
                reject(new Error("timeout"));
            }
        },1000*10);
    });
}

const lexerCommand=(path :string) => ["--stdin","--stdin-name",path, "--lexer", "--no-color", "--print-on-error","--print-json","--omit-json-warning"];
const parserCommand = (path :string) => ["--stdin","--stdin-name",path, "--no-color", "--print-on-error","--print-json","--omit-json-warning"];


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
               token.token=="cast"){
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
    assert(ast.ast)
    const prog = ast2ts.parseAST(ast.ast);
    ast2ts.walk(prog,(f,node)=>{
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
                                throw new Error("what happend?");
                            }
                            console.log("base -> "+n.base.usage);
                            n = n.base;
                            continue;
                        }
                        break;
                    case ast2ts.IdentUsage.define_variable:
                        break;
                    case ast2ts.IdentUsage.define_field:   
                    case ast2ts.IdentUsage.define_const:
                    case ast2ts.IdentUsage.define_enum_member:
                        locList.push({loc: node.loc,length: node.ident.length,index:6});
                        break;   
                    case ast2ts.IdentUsage.define_format:
                    case ast2ts.IdentUsage.define_enum:
                    case ast2ts.IdentUsage.reference_type:
                    case ast2ts.IdentUsage.define_cast_fn:
                    case ast2ts.IdentUsage.maybe_type:
                        locList.push({loc: node.loc,length: node.ident.length,index:7});
                        break;
                    case ast2ts.IdentUsage.define_fn:
                        locList.push({loc: node.loc,length: node.ident.length,index:8});
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
    const pos =  hover.offsetAt(params.position);
    const docInfo = getOrCreateDocumentInfo(hover);
    if(docInfo.prevNode===null) {
        return null;
    }
    let found :any;
    ast2ts.walk(docInfo.prevNode,(f,node)=>{
        if(node.loc.file!=1) {
            return; // skip other files
        }
        if(node.loc.pos.begin<=pos&&pos<=node.loc.pos.end){
            if(ast2ts.isIdent(node)){
                console.log(`found: ${node.ident} ${JSON.stringify(node.loc)}`)
                found = node;
                return;
            }
            console.log(`hit: ${node.node_type} ${JSON.stringify(node.loc)}`)
        }
        ast2ts.walk(node,f);
    });
    const color = (code :string,text :string) => {
        return `<span style="color: ${code};">${text}</span>`;
    }
    const makeHovoer = (ident :string,role :string) => {
        return {
            contents: {
                kind: MarkupKind.Markdown,
                value: `${ident}\n\n${role}`,
            },
        };
    }
    if(ast2ts.isIdent(found)){
        for(;;){
            switch (found.usage) {
                case ast2ts.IdentUsage.unknown:
                    return makeHovoer(found.ident,"unknown identifier");
                case ast2ts.IdentUsage.reference:
                    if(ast2ts.isIdent(found.base)){
                        found = found.base;
                        continue;
                    }
                    return makeHovoer(found.ident,"unspecified reference");
                case ast2ts.IdentUsage.define_variable:
                    return makeHovoer(found.ident,"variable");
                case ast2ts.IdentUsage.define_field:
                    // color of enum member
                    return makeHovoer(found.ident,"field");
                case ast2ts.IdentUsage.define_const:
                    return makeHovoer(found.ident,"constant");
                case ast2ts.IdentUsage.define_enum_member:
                    return makeHovoer(found.ident,"enum member");
                case ast2ts.IdentUsage.define_format:
                    return makeHovoer(found.ident,"format");
                case ast2ts.IdentUsage.define_enum:
                    return makeHovoer(found.ident,"enum");
                case ast2ts.IdentUsage.reference_type:
                    return makeHovoer(found.ident,"type");
                case ast2ts.IdentUsage.define_cast_fn:
                    return makeHovoer(found.ident,"cast function");
                case ast2ts.IdentUsage.maybe_type:
                    return makeHovoer(found.ident,"maybe type");
                case ast2ts.IdentUsage.define_fn:
                    return makeHovoer(found.ident,"function");
                default:
                    return makeHovoer(found.ident,"unknown identifier");
            }
        }   
    }
}

connection.onHover(hover)

const typeDefinitionHandler = async (params :TypeDefinitionParams) => {
    console.log(`textDocument/typeDefinition: ${JSON.stringify(params)}`);
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
            return false; // skip other files
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
                case ast2ts.IdentUsage.define_variable:
                case ast2ts.IdentUsage.define_field:
                case ast2ts.IdentUsage.define_const:
                case ast2ts.IdentUsage.define_enum_member:
                case ast2ts.IdentUsage.define_format:
                case ast2ts.IdentUsage.define_enum:
                case ast2ts.IdentUsage.define_cast_fn:
                case ast2ts.IdentUsage.maybe_type:
                case ast2ts.IdentUsage.define_fn:
                    return fileToLink(ident.loc,docInfo.prevFile!);
                default:
                    return null;
            }
        }
    }
    return null; 
}

connection.onTypeDefinition(typeDefinitionHandler);

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


