import {ast2ts} from "./ast";
import { Diagnostic, DiagnosticSeverity, MarkupContent, MarkupKind, Position, Range, SemanticTokensBuilder } from "vscode-languageserver";

namespace analyze {
export const unwrapType = (type :ast2ts.Type|null|undefined) :string => {
    if(!type){
        return "unknown";
    }
    if(ast2ts.isIdentType(type)){
       return unwrapType(type.base); 
    }
    return type.node_type;
}

export const bitSize = (bit :number|null|undefined) => {
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

const makeHover = (ident :string,role :string) => {
    return {
        contents: {
            kind: MarkupKind.Markdown,
            value: `### ${ident}\n\n${role}`,
        } as MarkupContent,
    };
}

export const analyzeHover = async (prevNode :ast2ts.Program, pos :number) =>{
    let found :any;
    ast2ts.walk(prevNode,(f,node)=>{
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
    if(found === null) {
        return null;
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
    return null;
}

export const analyzeDefinition = async (prevFile :ast2ts.AstFile, prevNode :ast2ts.Program,pos :number) => {
    let found :any;
    ast2ts.walk(prevNode,(f,node)=>{
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
        const range = {
            start: {line: loc.line-1, character: loc.col-1},
            end: {line: loc.line-1, character: loc.col-1},
        };
        const location  = {
            uri: path,
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
                    return fileToLink(ident.loc,prevFile);
                default:
                    return null;
            }
        }
    }
    return null; 
}

const stringEscape = (s :string) => {
    return s.replace(/\\/g,"\\\\").replace(/"/g,"\\\"").replace(/\n/g,"\\n").replace(/\r/g,"\\r");
}



export const makeDiagnostic =(positionAt :(pos :number)=>Position,errs :ast2ts.SrcError) => {
    return errs.errs.map((err)=> {
        if(err.loc.file != 1) {
            return null; // skip other files
        }
        const b = positionAt(err.loc.pos.begin);
        const e = positionAt(err.loc.pos.end);
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
}

export const analyzeSourceCode  = async (tokens :ast2ts.TokenFile,getAst: ()=>ast2ts.AstFile,positionAt :(pos :number)=>any,diagnostics :(s:Diagnostic[])=>void) =>{
    const mapForTokenTypes = new Map<ast2ts.TokenTag,string>([
        [ast2ts.TokenTag.comment,"comment"],
        [ast2ts.TokenTag.keyword,"keyword"],
        [ast2ts.TokenTag.bool_literal,"macro"],
        [ast2ts.TokenTag.int_literal,"number"],
        [ast2ts.TokenTag.str_literal,"string"],
        [ast2ts.TokenTag.ident,"variable"],
        [ast2ts.TokenTag.punct,"operator"],
    ]);
    const mapTokenTypesIndex = new Map<string,number>([
        ["comment",0],
        ["keyword",1],
        ["number",2],
        ["operator",3],
        ["string",4], 
        ["variable",5],
        ["enumMember",6],
        ["class",7],
        ["function",8],
        ["macro",9],
    ]); 
    type L = { readonly loc :ast2ts.Loc, readonly length :number, readonly index :number}
    const locList = new Array<L>();
    if(tokens.error !== null) {
        diagnostics(makeDiagnostic(positionAt,tokens.error));
    }
    if(tokens.tokens == null){
        return null;
    }
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
        })
        .filter((x)=>!removeList.includes(x))
        .forEach((x)=>{
            builder.push(x.loc.line-1,x.loc.col-1,x.length,x.index,0);
        });
        const semanticTokens = builder.build();
        console.log(`semanticTokens (parsed): ${JSON.stringify(semanticTokens)}`);
        console.timeEnd("semanticColoring")
        return semanticTokens;
    };
    let ast_ :ast2ts.AstFile;
    try {
        console.time("parse")
        ast_ = getAst();
    } catch(e :any) {
        console.timeEnd("parse")
        console.log(`error: ${e}`);
        return generateSemanticTokens();
    }
    console.timeEnd("parse")
    const ast = ast_;
    if(ast.error !== null) {
        diagnostics(makeDiagnostic(positionAt,ast.error));
    }
    else {
        diagnostics([]);
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
            if(line<0||col<0){{
                console.log(`line: ${line} col: ${col} ident: ${node.ident}`)
            }
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
    }});
    return generateSemanticTokens();
};

}
export {analyze}
