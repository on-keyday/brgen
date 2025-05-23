import {ast2ts} from "./ast";

namespace analyze {


export const bitSize = (bit :number|null|undefined) => {
    if(bit===undefined){
        return "unknown";
    }
    if(bit===null){
        return "dynamic";
    }
    if(bit%8===0){
        return `${bit/8} byte = ${bit} bit`;
    }
    return `${bit} bit`;
}

const makeHover = (ident :string,role :string) => {
    return {
        contents: {
            kind: "markdown",
            value: `### ${ident}\n\n${role}`,
        } ,
    } as const;
}

const mapOrderToString = (typ :ast2ts.OrderType,val  :number|null|undefined) => {
    if(val === undefined) {
        return "unknown";
    }
    if(val === null) {
        return "dynamic";
    }
    switch(typ) {
        case ast2ts.OrderType.byte:
            return `${val === 0? "big" : val === 1 ? "little" : "platform depended"} endian(${val})`;
        default: // bit order
            return `${val === 0? "msb" : val === 1 ? "lsb" : "platform depended"} first(${val})`;
    }
}

export const typeToString = (type :ast2ts.Type|null|undefined) :string => {
    if(!type){
        return "(unknown)";
    }
    const typ = type.node_type;
   
    if(ast2ts.isIdentType(type)) {
        const name = type.ident?.ident||"(null)";
        if(!type.base){
            return name + " (unknown)";
        }
        return name + " ("+typeToString(type.base)+")";
    }
    if(ast2ts.isStructUnionType(type)) {
        return "struct union (size: "+bitSize(type.bit_size)+")";
    }
    if(ast2ts.isStructType(type)) {
        return "struct (size: "+bitSize(type.bit_size)+")";
    }
    if(ast2ts.isUnionType(type)) {
        return "union (size: "+bitSize(type.bit_size)+")";
    }
    if(ast2ts.isEnumType(type)) {
        let b = "enum (size: "+bitSize(type.bit_size);
        if(type.base?.base_type) {
            b += " base: "+typeToString(type.base.base_type);
        }
        b += ")";
        return b;
    }
    if(ast2ts.isIntType(type)) {
        const sign = type.is_signed?"s":"u";
        const endian = type.endian == ast2ts.Endian.big?"b":
                       type.endian == ast2ts.Endian.little?"l":"";
        return sign+endian+type.bit_size?.toString()||"unknown";
    }
    if(ast2ts.isArrayType(type)) {
        if(type.length_value) {
            return "["+type.length_value.toString()+"] "+typeToString(type.element_type);
        }
        return "[] "+typeToString(type.element_type);
    }
    if(ast2ts.isRangeType(type)) {
        const op = type.range?.op === ast2ts.BinaryOp.range_inclusive ? "range_inclusive" : "range_exclusive";
        if(type.base_type) {
            return op+" "+typeToString(type.base_type);
        }
        return op+" (any)";
    }
    if(ast2ts.isFloatType(type)) {
        return "f"+type.bit_size?.toString()||"unknown";
    }
    if(ast2ts.isFunctionType(type)) {
        let b = "fn (";
        if(type.parameters.length > 0) {
            b += type.parameters.map((x)=>typeToString(x)).join(", ");
        }
        b += ") -> "+typeToString(type.return_type);
        return b;
    }
    if(ast2ts.isVoidType(type)) {
        return "void";
    }
    if(ast2ts.isBoolType(type)) {
        return "bool";
    }
   
    return typ;
}

export const analyzeHover = async (prevNode :ast2ts.ParseResult, pos :number) =>{
    let found :any;
    console.time("walk hover");
    const len = prevNode.node.length;
    for(let i = 0;i<len;i++){
        const node = prevNode.node[i];
        if(node.loc.file!=1) {
            console.log("prevent file boundary: "+node.loc.file)
            continue; // skip other files
        }
        if(node.loc.pos.begin<=pos&&pos<=node.loc.pos.end){
            if(ast2ts.isIdent(node)){
                console.log(`found: ${node.ident} ${JSON.stringify(node.loc)}`)
                found = node;
                break;
            }
            else if(ast2ts.isIntLiteral(node)) {
                console.log(`found: ${node.node_type} ${JSON.stringify(node.loc)}`)
                found = node;
                break;
            }
            else if(ast2ts.isStrLiteral(node)) {
                console.log(`found: ${node.node_type} ${JSON.stringify(node.loc)}`)
                found = node;
                break;
            }
            else if(ast2ts.isRegexLiteral(node)) {
                console.log(`found: ${node.node_type} ${JSON.stringify(node.loc)}`)
                found = node;
                break;
            }
            else if(ast2ts.isCharLiteral(node)) {
                console.log(`found: ${node.node_type} ${JSON.stringify(node.loc)}`)
                found = node;
                break;
            }
            else if(ast2ts.isAssert(node)) {
                console.log(`found: ${node.node_type} ${JSON.stringify(node.loc)}`)
                found = node;
                break;
            }
            else if(ast2ts.isMetadata(node)) {
                console.log(`found: ${node.node_type} ${JSON.stringify(node.loc)}`)
                found = node;
                break;
            }
            else if(ast2ts.isType(node)&&node.is_explicit&&node.node_type!== "ident_type"){
                console.log(`found: ${node.node_type} ${JSON.stringify(node.loc)}`)
                found = node;
                break;
            }
            else if(ast2ts.isMatch(node)) {
                console.log(`found: ${node.node_type} ${JSON.stringify(node.loc)}`)
                found = node;
                break;
            }
            else if(ast2ts.isIf(node)) {
                console.log(`found: ${node.node_type} ${JSON.stringify(node.loc)}`)
                found = node;
                break;
            }
            else if(ast2ts.isSpecifyOrder(node)) {
                console.log(`found: ${node.node_type} ${JSON.stringify(node.loc)}`)
                found = node;
                break;
            }
            console.log(`hit: ${node.node_type} ${JSON.stringify(node.loc)}`)
        }
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
    }
    console.timeEnd("walk hover");
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
                case ast2ts.IdentUsage.reference_member_type:
                    if(ast2ts.isMemberAccess(ident.base)){
                        if(ast2ts.isIdent(ident.base.base)){
                            ident = ident.base.base;
                            continue;
                        }
                    }
                    return makeHover(ident.ident,`unspecified member ${ident.usage == ast2ts.IdentUsage.reference_member_type ? "type": ""}reference`);
                case ast2ts.IdentUsage.define_variable:
                    return makeHover(ident.ident,`variable (type: ${typeToString(ident.expr_type)}, size: ${bitSize(ident.expr_type?.bit_size)} constant level: ${ident.constant_level})`);
                case ast2ts.IdentUsage.define_const:
                    return makeHover(ident.ident,`constant (type: ${typeToString(ident.expr_type)}, size: ${bitSize(ident.expr_type?.bit_size)} constant level: ${ident.constant_level})`);
                case ast2ts.IdentUsage.define_field:
                    if(ast2ts.isField(ident.base)){
                        const sizeAlignTarget = ident.base.arguments?.type_map &&
                            ast2ts.isIdentType(ident.base.field_type)&&ast2ts.isEnumType(ident.base.field_type.base)
                        ? ident.base.arguments.type_map.type_literal : ident.base.field_type;
                        return makeHover(ident.ident, 
                            `
+ field 
    + type: ${typeToString(ident.base.field_type)}
    + size: ${bitSize(sizeAlignTarget?.bit_size)}
    + offset(from begin): ${bitSize(ident.base.offset_bit)}
    + offset(from recent fixed): ${bitSize(ident.base.offset_recent)}
    + offset(from end): ${bitSize(ident.base.tail_offset_bit)}
    + offset(from recent fixed): ${bitSize(ident.base.tail_offset_recent)}
    + align: ${ident.base.bit_alignment}, 
    + eventual_align: ${ident.base.eventual_bit_alignment}
    + type align: ${sizeAlignTarget?.bit_alignment||"unknown"}
    + follow: ${ident.base.follow}
    + eventual_follow: ${ident.base.eventual_follow}
    ${ident.base.is_state_variable?"+ state variable\n":""}`);
                    }
                    return makeHover(ident.ident,"field");
                case ast2ts.IdentUsage.define_enum_member:
                    if(ast2ts.isEnumType(ident.expr_type)) {
                        return makeHover(ident.ident,`enum member (type: ${ident.expr_type.base?.ident?.ident||"unknown"}, size: ${bitSize(ident.expr_type?.bit_size)})`);
                    }
                    return makeHover(ident.ident,"enum member");
                case ast2ts.IdentUsage.define_format:
                    if(ast2ts.isFormat(ident.base)){          
                        const convertIdentType = (ident :ast2ts.IdentType) => {
                            if(ident.import_ref !== null) {
                                if(ast2ts.isIdent(ident.import_ref.target)){
                                    return ident.import_ref.target.ident+ "." + (ident.ident?.ident||"(null)") + " (imported)";
                                }
                                else {
                                    return "(unknown)."+(ident.ident?.ident || "(null)") + " (imported)";
                                }
                            }
                            return ident.ident?.ident||"(null)";
                        }              
                        return makeHover(ident.ident,
                        `
+ format 
    + size: ${bitSize(ident.base.body?.struct_type?.bit_size)}
    + fixed header size: ${bitSize(ident.base.body?.struct_type?.fixed_header_size)}
    + fixed tail size: ${bitSize(ident.base.body?.struct_type?.fixed_tail_size)}
    + algin: ${ident.base.body?.struct_type?.bit_alignment||"unknown"}
    ${ident.base.body?.struct_type?.non_dynamic_allocation?"+ non_dynamic\n    ":""}${ident.base.body?.struct_type?.recursive?"+ recursive\n":""}
    ${ident.base.depends.length > 0 ?`+ depends: ${ident.base.depends.map((x)=>convertIdentType(x)).filter((elem, index, self) => self.indexOf(elem) === index).join(", ")}\n`:""}
    ${ident.base.state_variables.length > 0 ?`+ state variables: ${ident.base.state_variables.map((x)=>x.ident?.ident||"(null)").filter((elem, index, self) => self.indexOf(elem) === index).join(", ")}\n`:""}
    ${ident.base.encode_fn?"+ custom encode\n":""}
    ${ident.base.decode_fn?"+ custom decode\n":""}
    ${(ident.base.body?.metadata.length??0) > 0 ?`+ metadata: ${ident.base.body?.metadata.map((x)=>x.name).join(", ")}\n`:""}
    ${(ident.base.cast_fns.length || 0) > 0 ?`+ cast functions: ${ident.base.cast_fns.map((x)=> typeToString(x.return_type) ).join(", ")}\n`:""}
    + block trait: ${ident.base.body?.block_traits && ast2ts.BlockTraitToString(ident.base.body?.block_traits) || "none"}
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
                case ast2ts.IdentUsage.define_fn:
                    if(ast2ts.isFunction(ident.base)){
                    return makeHover(ident.ident,`
+ ${ident.base.is_cast?"cast function":"function"}
    + block trait: ${ident.base.body?.block_traits && ast2ts.BlockTraitToString(ident.base.body?.block_traits) || "none"}`);
                    }
                    return makeHover(ident.ident,ident.usage == ast2ts.IdentUsage.define_cast_fn?"cast function":"function");
                case ast2ts.IdentUsage.maybe_type:
                    return makeHover(ident.ident,"maybe type");
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
    else if(ast2ts.isType(found)){
        return makeHover("type",`type (type: ${found.node_type || "unknown"}, size: ${bitSize(found.bit_size)}, align: ${found.bit_alignment})`);
    }
    else if(ast2ts.isMatch(found)){
        return makeHover("match",`match (exhaustive: ${found.struct_union_type?.exhaustive||false},${found.trial_match?" trial_match,":""} expr_type: ${found.expr_type?.node_type||"unknown"}, size: ${bitSize(found.struct_union_type?.bit_size)} align: ${found.struct_union_type?.bit_alignment})`);
    }
    else if(ast2ts.isIf(found)) {
        if(found.struct_union_type !== null) {
            return makeHover("if",`if (exhaustive: ${found.struct_union_type.exhaustive||false} size: ${bitSize(found.struct_union_type.bit_size)} align: ${found.struct_union_type.bit_alignment})`);
        }
    }
    else if(ast2ts.isStrLiteral(found)){
        return makeHover("string",`string literal (length: ${found.length})`);
    }
    else if(ast2ts.isRegexLiteral(found)){
        return makeHover("regex",`regex literal (pattern: ${found.value})`);
    }
    else if (ast2ts.isCharLiteral(found)) {
        return makeHover("char",`char literal (code: ${found.code} = 0x${found.code.toString(16)})`);
    }
    else if(ast2ts.isMetadata(found)){
        return makeHover("metadata",`metadata (name: ${found.name} value count: ${found.values.length})`);
    }
    else if(ast2ts.isSpecifyOrder(found)){
        return makeHover("specify_order",`specify_order (order_type: ${found.order_type} order: ${mapOrderToString(found.order_type,found.order_value)})`);
    }
    else if(ast2ts.isIntLiteral(found)){
        const bigInt = BigInt(found.value);
        return makeHover("int",`int literal (value: ${found.value} = ${bigInt.toString()} = 0x${bigInt.toString(16)} type: ${typeToString(found.expr_type)} size: ${bitSize(found.expr_type?.bit_size)})`);
    }
    return null;
}

export const analyzeDefinition = async (prevFile :ast2ts.AstFile, prevNode :ast2ts.ParseResult,pos :number) => {
    let found :any;
    const len = prevNode.node.length;
    for(let i = 0;i<len;i++){
        const node = prevNode.node[i];
        if(node.loc.file!=1) {
            console.log("prevent file boundary: "+node.loc.file)
            continue; // skip other files
        }
        if(node.loc.pos.begin<=pos&&pos<=node.loc.pos.end){
            if(ast2ts.isIdent(node)){
                console.log(`found: ${node.ident} ${JSON.stringify(node.loc)}`)
                found = node;
                break;
            }
            console.log(`hit: ${node.node_type} ${JSON.stringify(node.loc)}`)
        }
    }
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
                case ast2ts.IdentUsage.reference_member_type:
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

export interface PosStub {
    line: number;
    character: number;
}

export interface RangeStub {
    start: PosStub;
    end: PosStub;
}

export const enum DiagnosticSeverityStub {
    Error = 1,
    Warning = 2,
}

export interface DiagnosticStub {
    range: RangeStub;
    message: string;
    severity: DiagnosticSeverityStub;
    source: string;
}

export const makeDiagnostic =(positionAt :(pos :number)=>PosStub,errs :ast2ts.SrcError) => {
    return errs.errs.map((err)=> {
        if(err.loc.file != 1) {
            return null; // skip other files
        }
        const b = positionAt(err.loc.pos.begin);
        const e = positionAt(err.loc.pos.end);
        const range  = {
            start: b,
            end: e,
        } as const;
        return {
            severity: err.warn ? DiagnosticSeverityStub.Warning : DiagnosticSeverityStub.Error,
            range: range,
            message: err.msg,
            source: 'brgen-lsp',
        } as DiagnosticStub;
    }).filter((x)=>x!==null) as DiagnosticStub[];
}

export const legendMapping = [
    "comment",
    "keyword",
    "number",
    "operator",
    "string",
    "variable",
    "enumMember",
    "class",
    "function",
    "macro",
    "regexp",
];

export type LocMap = { readonly loc :ast2ts.Loc, readonly length :number, readonly index :number}

class STBuilder {
    prevLine :number = 0;
    prevChar :number = 0;
    data :number[] = [];

    push(line :number, char :number, length :number, tokenType :number, ignore: number) {
        let pushLine = line;
        let pushChar = char;
        if (this.data.length > 0) {
            pushLine -= this.prevLine;
            if (pushLine === 0) {
                pushChar -= this.prevChar;
            }
        }
        this.prevLine = line;
        this.prevChar = char;
        this.data.push(pushLine, pushChar, length, tokenType, ignore);
    }

    build() {
        return {
            resultId: undefined,
            data: this.data,
        }
    }
}


export const generateSemanticTokens = (locList :Array<LocMap>) => {
    const builder = new STBuilder();
    const removeList = new Array<LocMap>();
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
    return semanticTokens;
};

export interface SemTokensStub {
    resultId? :string;
    data :number[];
}

export const analyzeSourceCode  = async (prevSemanticTokens :SemTokensStub|null,  tokens :ast2ts.TokenFile,getAst: ()=>Promise<{file: ast2ts.AstFile,ast: ast2ts.ParseResult|null}>,positionAt :(pos :number)=>PosStub,diagnostics :(s:DiagnosticStub[])=>void) =>{
    const mapForTokenTypes = new Map<ast2ts.TokenTag,string>([
        [ast2ts.TokenTag.comment,"comment"],
        [ast2ts.TokenTag.keyword,"keyword"],
        [ast2ts.TokenTag.bool_literal,"macro"],
        [ast2ts.TokenTag.int_literal,"number"],
        [ast2ts.TokenTag.str_literal,"string"],
        [ast2ts.TokenTag.partial_str_literal,"string"],
        [ast2ts.TokenTag.regex_literal,"string"],
        [ast2ts.TokenTag.partial_regex_literal,"string"],
        [ast2ts.TokenTag.char_literal,"string"],
        [ast2ts.TokenTag.partial_char_literal,"string"],
        [ast2ts.TokenTag.ident,"variable"],
        [ast2ts.TokenTag.punct,"operator"],
    ]);
    const mapTokenTypesIndex = new Map<string,number>(legendMapping.map((x,i)=>[x,i] as [string,number])); 
    let locList = new Array<LocMap>();
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
    
    let ast_ :ast2ts.AstFile;
    let prog_ :ast2ts.ParseResult|null;
    try {
        console.time("parse")
        const l = await getAst();
        ast_ = l.file;
        prog_ = l.ast;
    } catch(e :any) {
        console.timeEnd("parse")
        console.error(`error: `,e);
        diagnostics([]);
        return generateSemanticTokens(locList);
    }
    console.timeEnd("parse")
    const ast = ast_;
    if(ast.error !== null) {
        diagnostics(makeDiagnostic(positionAt,ast.error));
    }
    else {
        diagnostics([]);
    }
    if(prog_ === null) {
        if(prevSemanticTokens !== null) {
            return prevSemanticTokens;
        }
        return generateSemanticTokens(locList);
    }
    const prog = prog_;
    console.time("walk ast");
    const len=prog.node.length;
    for(let i=0;i<len;i++){
        const node = prog.node[i];
        if(node.loc.file!=1) {
            console.log("prevent file boundary: "+node.loc.file)
            continue; // skip other files
        }
        if(ast2ts.isIdent(node)){
            const line = node.loc.line-1;
            const col = node.loc.col-1;
            if(line<0||col<0){
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
                    case ast2ts.IdentUsage.reference_member_type:
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
        else if(ast2ts.isBinary(node)&&node.op == ast2ts.BinaryOp.in_assign) {
            locList.push({loc: node.loc,length: 2,index:9});
        }
        else if(ast2ts.isRegexLiteral(node)){
            // remove tokenized string
            locList =  locList.filter((l)=>{
                return !(l.loc.pos.begin >= node.loc.pos.begin && l.loc.pos.end <= node.loc.pos.end);
            });
            locList.push({loc: node.loc,length: node.value.length,index:4});
        }
    }
    console.timeEnd("walk ast");
    return generateSemanticTokens(locList);
};


export interface PosStub {
    line: number;
    character: number;
}

export interface RangeStub {
    start: PosStub;
    end: PosStub;
}

export const enum SymbolKindStub {
    Variable = 13,
}

export interface DocumentSymbolStub {
    name: string;
    kind: SymbolKindStub;
    range: RangeStub;
    selectionRange: RangeStub;
    children? :DocumentSymbolStub[];
}

export const analyzeSymbols = (scope :ast2ts.Scope) :DocumentSymbolStub[] => {
    var symbols :DocumentSymbolStub[] = [];
    let preSym :DocumentSymbolStub | null = null;
    for(let sc=scope as ast2ts.Scope | null;sc != null;sc = scope.next) {
        for(const s of scope.ident) {
            let sym : DocumentSymbolStub = {
                name: s.ident,
                kind: SymbolKindStub.Variable,
                range: {
                    start: {line: s.loc.line-1, character: s.loc.col-1},
                    end: {line: s.loc.line-1, character: s.loc.col-1},
                },
                selectionRange: {
                    start: {line: s.loc.line-1, character: s.loc.col-1},
                    end: {line: s.loc.line-1, character: s.loc.col-1},
                },
            };
            symbols.push(sym);
            preSym = sym;
        }  
        if(sc.branch !== null) {
            if(preSym !== null) {
                preSym.children = analyzeSymbols(sc.branch);
            }
            else {
                symbols = symbols.concat(analyzeSymbols(sc.branch));
            }
        }
    }
    return symbols;
}


}

export {analyze}
