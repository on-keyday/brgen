/* --------------------------------------------------------------------------------------------
 * Copyright (c) Microsoft Corporation. All rights reserved.
 * Licensed under the MIT License. See License.txt in the project root for license information.
 * ------------------------------------------------------------------------------------------ */
import {
    createConnection,
    TextDocuments,
    Diagnostic,
    DiagnosticSeverity,
    ProposedFeatures,
    InitializeParams,
    DidChangeConfigurationNotification,
    CompletionItem,
    CompletionItemKind,
    TextDocumentPositionParams,
    TextDocumentSyncKind,
    InitializeResult,
    SemanticTokensLegend,
    SemanticTokensBuilder,
    SemanticTokenTypes,
    SemanticTokenModifiers
} from 'vscode-languageserver/node';

import { TextDocument } from 'vscode-languageserver-textdocument';

import * as fs from "fs";
import {exec} from "child_process";
import * as url from "url";
import { ast2ts } from "ast2ts";
import assert from 'node:assert/strict';
import { randomUUID } from 'crypto';





const CWD=process.cwd();
//TODO(on-keyday): replace path to src2json
const PATH_TO_SRC2JSON =`C:/workspace/shbrgen/brgen/build/tool/src2json${(process.platform === "win32" ? ".exe" : "")}`

// Create a connection for the server, using Node's IPC as a transport.
// Also include all preview / proposed LSP features.
let connection = createConnection(ProposedFeatures.all);

// Create a simple text document manager.
let documents: TextDocuments<TextDocument> = new TextDocuments(TextDocument);

let hasConfigurationCapability: boolean = false;
let hasWorkspaceFolderCapability: boolean = false;
let hasDiagnosticRelatedInformationCapability: boolean = false;

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
    hasDiagnosticRelatedInformationCapability = !!(
        capabilities.textDocument &&
        capabilities.textDocument.publishDiagnostics &&
        capabilities.textDocument.publishDiagnostics.relatedInformation
    );

    const result: InitializeResult = {
        capabilities: {
            textDocumentSync: TextDocumentSyncKind.Incremental,
            // Tell the client that this server supports code completion.
            completionProvider: {
                resolveProvider: true
            }
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

const tokenizeSource  = (doc :TextDocument) =>{
    const path = url.fileURLToPath(doc.uri)
    exec(`${PATH_TO_SRC2JSON} ${path} --lexer --no-color --print-on-error --print-json`,(err,stdout,stderr)=>{
        if(err){
            console.log(`err: ${err}`);
            return;
        }
        const tokens=JSON.parse(stdout);
        console.log(`stdout: ${stdout}`);
        console.log(`json: ${tokens}`);
        console.log(`URI: ${doc.uri}`);
        if(!ast2ts.isTokenFile(tokens)){
            console.log(`not token file`);
            return;
        }
        if(tokens.error) {
            console.log(`token error: ${tokens.error}`);
            return;
        }
        assert(tokens.tokens);
        const builder = new SemanticTokensBuilder();
        const mapForTokenTypes = new Map<ast2ts.TokenTag,SemanticTokenTypes>([
            [ast2ts.TokenTag.comment,SemanticTokenTypes.comment],
            [ast2ts.TokenTag.keyword,SemanticTokenTypes.keyword],
            [ast2ts.TokenTag.bool_literal,SemanticTokenTypes.keyword],
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
        ]); 
        tokens.tokens.forEach((token:ast2ts.Token)=>{
            const type = mapForTokenTypes.get(token.tag);
            if(type===undefined){
                return;
            }
            const index = mapTokenTypesIndex.get(type);
            if(index===undefined){
                return;
            }
            builder.push(token.loc.line,token.loc.col,token.token.length,index,1);
        });
        const legend:SemanticTokensLegend = {
            tokenTypes: [
                'comment',
                'keyword',
                'number',
                'operator',
                'string',
                'variable',
            ],
            tokenModifiers: [
                "declaration",
            ],
        };
        const semanticTokens = builder.build();
        console.log(`semanticTokens: ${JSON.stringify(semanticTokens)}`);
        connection.sendNotification('brgen-lsp/tokenized', {
            textDocument: {
                uri: doc.uri,
            },
            legend: legend,
            data: semanticTokens.data,
        });
    });
};


// The example settings
interface ExampleSettings {
    maxNumberOfProblems: number;
}

// The global settings, used when the `workspace/configuration` request is not supported by the client.
// Please note that this is not the case when using this server with the client provided in this example
// but could happen with other clients.
const defaultSettings: ExampleSettings = { maxNumberOfProblems: 1000 };
let globalSettings: ExampleSettings = defaultSettings;

// Cache the settings of all open documents
let documentSettings: Map<string, Thenable<ExampleSettings>> = new Map();

connection.onDidChangeConfiguration(change => {
    if (hasConfigurationCapability) {
        // Reset all cached document settings
        documentSettings.clear();
    } else {
        globalSettings = <ExampleSettings>(
            (change.settings.languageServerExample || defaultSettings)
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
            section: 'languageServerExample'
        });
        documentSettings.set(resource, result);
    }
    return result.then((x)=>{
        if(x===null){
            return defaultSettings;
        }
        return x;
    },(_)=>{
        return defaultSettings;
    });
}

// Only keep settings for open documents
documents.onDidClose(e => {
    documentSettings.delete(e.document.uri);
});

// The content of a text document has changed. This event is emitted
// when the text document first opened or when its content has changed.
documents.onDidChangeContent(change => {
    // validateTextDocument(change.document);
    tokenizeSource(change.document);
});


connection.onDidChangeWatchedFiles(_change => {
    // Monitored files have change in VS Code
    connection.console.log('We received a file change event');
});


// This handler provides the initial list of the completion items.
connection.onCompletion(
    (_textDocumentPosition: TextDocumentPositionParams): CompletionItem[] => {
        // The pass parameter contains the position of the text document in
        // which code complete got requested. For the example we ignore this
        // info and always provide the same completion items.
        return [
            {
                label: 'TypeScript',
                kind: CompletionItemKind.Text,
                data: 1
            },
            {
                label: 'JavaScript',
                kind: CompletionItemKind.Text,
                data: 2
            }
        ];
    }
);

// This handler resolves additional information for the item selected in
// the completion list.
connection.onCompletionResolve(
    (item: CompletionItem): CompletionItem => {
        if (item.data === 1) {
            item.detail = 'TypeScript details';
            item.documentation = 'TypeScript documentation';
        } else if (item.data === 2) {
            item.detail = 'JavaScript details';
            item.documentation = 'JavaScript documentation';
        }
        return item;
    }
);

// Make the text document manager listen on the connection
// for open, change and close text document events
documents.listen(connection);

const REGISTER_ID= randomUUID()

connection.sendNotification('client/registerCapability', {
    registrations: [
        {
            id: REGISTER_ID,
            method: 'textDocument/didChange',
            registerOptions: {
                documentSelector: [
                    { scheme: 'file', language: 'brgen', pattern: '*.bgn' },
                ],
            },
        },
    ],
});


// Listen on the connection
connection.listen();
