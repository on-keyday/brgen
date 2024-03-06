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
    SemanticTokens,
    HoverParams,
    DefinitionParams,
    Diagnostic, 
    DiagnosticSeverity,
} from 'vscode-languageserver/node';

import { Range, TextDocument } from 'vscode-languageserver-textdocument';

import {execFile} from "child_process";
import * as url from "url";
import { ast2ts,analyze } from "ast2ts";






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
            //documentSymbolProvider: true,
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
    prevNode :ast2ts.Program | null = null;
    prevText :string | null = null;

    constructor(uri :string){
        this.uri = uri;
    }
}

let documentInfos = new Map<string,DocumentInfo>();




const showDiagnostic = (doc :TextDocument, diagnostics :Diagnostic[])=>{
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
    console.timeEnd("tokenize")
    const res =await analyze.analyzeSourceCode(docInfo.prevSemanticTokens,tokens_,async()=>{
        let ast =await execSrc2JSON(settings.src2json,parserCommand(path),text,ast2ts.isAstFile);
        docInfo.prevFile = ast;
        if(ast.ast !== null) {
            docInfo.prevNode = ast2ts.parseAST(ast.ast);
            return {file:ast,ast:docInfo.prevNode};
        }
        return {file:ast,ast:null};
    },(pos)=>{return doc.positionAt(pos)},(d)=>{showDiagnostic(doc,d)})
    console.timeEnd("semanticColoring")
    return res;
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
        console.log("prevNode is null");
        return null;
    }
    return  analyze.analyzeHover(docInfo.prevNode,pos);
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
        console.log("prevNode is null");
        return null;
    }
    const def= await analyze.analyzeDefinition(docInfo.prevFile!,docInfo.prevNode,pos)
    if(def===null){
        console.log("def is null");
        return null;
    }
    def.uri = url.pathToFileURL(def.uri).toString();
    return def;
}

connection.onDefinition(definitionHandler);

connection.onDocumentSymbol(async (params) =>{
    console.log(`textDocument/documentSymbol: ${JSON.stringify(params)}`);
    const doc = documents.get(params?.textDocument?.uri);
    if(doc===undefined){
        console.log(`document ${params?.textDocument?.uri} is not found`);
        return null; 
    }
    const docInfo = getOrCreateDocumentInfo(doc);
    if(docInfo.prevNode===null) {
        return null;
    }
    return analyze.analyzeSymbols(docInfo.prevNode?.global_scope!);
})


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


