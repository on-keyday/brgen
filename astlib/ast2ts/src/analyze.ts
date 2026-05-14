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

const makeHover = (ident :string,role :string,comments :string[] = []) => {
    return {
        contents: {
            kind: "markdown",
            value: `### ${ident}\n\n${role}\n\n${comments.join("\n")}`,
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
    if(ast2ts.isGenericType(type)) {
        const base = type.base_type?.ident?.ident || "(null)";
        const args = type.type_arguments.map((a)=>typeToString(a)).join(", ");
        return base + "[" + args + "]";
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
        const sign = type.is_signed?"i":"u";
        const endian = type.endian == ast2ts.Endian.big?"b":
                       type.endian == ast2ts.Endian.little?"l":"";
        let type_str = sign+endian+type.bit_size?.toString()||"unknown";
        type_str += `(${type.is_signed ? "signed" : "unsigned"} ${bitSize(type.bit_size)} ${type.endian == ast2ts.Endian.big ? "big-endian" : type.endian == ast2ts.Endian.little ? "little-endian" : "context dependent endian (default: big-endian)"})`;
        return type_str;
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

const collectComments = (node :ast2ts.Node) :string[] => {
    const comments :string[] = [];
    const addCommentNode = (commentNode :ast2ts.Node) => {
        if(ast2ts.isComment(commentNode)) {
            comments.push(commentNode.comment);
        }
        if(ast2ts.isCommentGroup(commentNode)) {
            commentNode.comments.forEach((c)=>{
                comments.push(c.comment);
            })
        }
    }
    if(ast2ts.isMember(node) && node.comment) {
        addCommentNode(node.comment);
    }
    if(ast2ts.isField(node) && node.follow_comment) {
        addCommentNode(node.follow_comment);
    }
    // remove `#` from the beginning of comment if exists
    return comments.map((c)=>c.startsWith("#")?c.substring(1).trim():c.trim());
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
            else if (ast2ts.isSizeof(node)) {
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
    ${ident.base.is_state_variable?"+ state variable\n":""}`, collectComments(ident.base));
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
                        const fmt = ident.base;
                        let suffix = "";
                        let genericLine = "";
                        if(fmt.type_parameters.length > 0) {
                            const params = fmt.type_parameters.map((tp)=>tp.ident?.ident||"(null)").join(", ");
                            suffix = `[${params}]`;
                            genericLine = `+ generic template (type parameters: ${params})\n    `;
                        }
                        else if(fmt.generic_base !== null) {
                            const argList = fmt.generic_arguments.map(typeToString).join(", ");
                            suffix = `[${argList}]`;
                            genericLine = `+ instantiated from ${fmt.generic_base?.ident?.ident||"(null)"} with [${argList}]\n    `;
                        }
                        return makeHover(ident.ident + suffix,
                        `
+ format
    ${genericLine}+ size: ${bitSize(fmt.body?.struct_type?.bit_size)}
    + fixed header size: ${bitSize(fmt.body?.struct_type?.fixed_header_size)}
    + fixed tail size: ${bitSize(fmt.body?.struct_type?.fixed_tail_size)}
    + algin: ${fmt.body?.struct_type?.bit_alignment||"unknown"}
    ${fmt.body?.struct_type?.non_dynamic_allocation?"+ non_dynamic\n    ":""}${fmt.body?.struct_type?.recursive?"+ recursive\n":""}
    ${fmt.depends.length > 0 ?`+ depends: ${fmt.depends.map((x)=>convertIdentType(x)).filter((elem, index, self) => self.indexOf(elem) === index).join(", ")}\n`:""}
    ${fmt.state_variables.length > 0 ?`+ state variables: ${fmt.state_variables.map((x)=>x.ident?.ident||"(null)").filter((elem, index, self) => self.indexOf(elem) === index).join(", ")}\n`:""}
    ${fmt.encode_fn?"+ custom encode\n":""}
    ${fmt.decode_fn?"+ custom decode\n":""}
    ${(fmt.body?.metadata.length??0) > 0 ?`+ metadata: ${fmt.body?.metadata.map((x)=>x.name).join(", ")}\n`:""}
    ${(fmt.cast_fns.length || 0) > 0 ?`+ cast functions: ${fmt.cast_fns.map((x)=> typeToString(x.return_type) ).join(", ")}\n`:""}
    ${fmt.body?.struct_type?.type_map ? `+ type mapped: ${typeToString(fmt.body.struct_type.type_map.type_literal)}`: ""}
    + block trait: ${fmt.body?.block_traits && ast2ts.BlockTraitToString(fmt.body?.block_traits) || "none"}
`, collectComments(fmt));
                    }
                    return makeHover(ident.ident,"format");
                case ast2ts.IdentUsage.define_enum:
                    if(ast2ts.isEnum(ident.base)){
                        return makeHover(ident.ident,`enum (size: ${bitSize(ident.base.base_type?.bit_size)}, align: ${ident.base?.base_type?.bit_alignment || "unknown"})`);
                    }
                    return makeHover(ident.ident,"enum");
                case ast2ts.IdentUsage.define_state:
                    return makeHover(ident.ident,"state");
                case ast2ts.IdentUsage.define_type_parameter:
                    return makeHover(ident.ident,"type parameter");
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
                    return makeHover(ident.ident,"function parameter (type: "+typeToString(ident.expr_type)+")");
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
    else if(ast2ts.isSizeof(found)){
        return makeHover("sizeof",`sizeof (type: ${typeToString(found.target?.expr_type)}, size: ${bitSize(found.evaluated_value !== null ? found.evaluated_value * 8 : null)})`);
    }
    else if(ast2ts.isType(found)){
        let sign = "";
        if(ast2ts.isIntType(found)) {
            sign = found.is_signed?"signed":"unsigned";
        }
        return makeHover("type",`type (type: ${found.node_type || "unknown"}${sign ? `(${sign})` : ""}, size: ${bitSize(found.bit_size)}, align: ${found.bit_alignment})`);
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

export const analyzeDefinition = async (prevFile :ast2ts.AstFile, prevNode :ast2ts.ParseResult,pos :number,typeDef :boolean) => {
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
        if(typeDef){
            if(!ident.expr_type){
                return null;
            }
            if(ast2ts.isIdentType(ident.expr_type)){
                if(ident.expr_type.ident !== null){
                    ident = ident.expr_type.ident;
                }
                else{
                    return null;
                }
            }
            else{
                return null;
            }
        }
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

export const DiagnosticSeverityStub = {
    Error: 1,
    Warning: 2,
} as const;
export type DiagnosticSeverityStub = (typeof DiagnosticSeverityStub)[keyof typeof DiagnosticSeverityStub];

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
        [ast2ts.TokenTag.partial_int_literal,"number"],
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
    let prevToken :ast2ts.Token|null = null;
    tokens.tokens.forEach((token:ast2ts.Token)=>{
        console.log(`token: ${stringEscape(token.token)} ${token.tag} ${token.loc.line} ${token.loc.col}`)
        if(token.tag===ast2ts.TokenTag.keyword){
            if(token.token==="input"||token.token==="output"||token.token=="config"||
               token.token=="fn"||token.token=="format"||token.token=="enum"||
               token.token=="state"){
                locList.push({loc:token.loc,length: token.token.length,index:9});
                prevToken = token;
                return;
            }
        }
        if(prevToken !== null && prevToken.tag === ast2ts.TokenTag.keyword && 
            prevToken.token === "fn" && token.tag === ast2ts.TokenTag.ident 
        ) {
            locList.push({ loc: token.loc, length: token.token.length, index: 8 });
            prevToken = token;
            return;
        }
        else if(prevToken !== null && prevToken.tag === ast2ts.TokenTag.keyword && 
            (prevToken.token === "format"||
                prevToken.token === "enum"||prevToken.token === "state"
            ) && token.tag === ast2ts.TokenTag.ident) {
            locList.push({ loc: token.loc, length: token.token.length, index: 7 });
            prevToken = token;
            return;
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
        prevToken = token;
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
            // Clamp ident length to source span so that monomorphized clones
            // (whose ident string is longer than the original source text)
            // do not paint beyond the actual token.
            const srcSpan = node.loc.pos.end - node.loc.pos.begin;
            const identLen = srcSpan > 0 ? Math.min(node.ident.length, srcSpan) : node.ident.length;
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
                        locList.push({loc: node.loc,length: identLen,index:6});
                        break;
                    case ast2ts.IdentUsage.define_format:
                    case ast2ts.IdentUsage.define_enum:
                    case ast2ts.IdentUsage.define_state:
                    case ast2ts.IdentUsage.reference_type:
                    case ast2ts.IdentUsage.define_cast_fn:
                    case ast2ts.IdentUsage.define_type_parameter:
                    case ast2ts.IdentUsage.maybe_type:
                        locList.push({loc: node.loc,length: identLen,index:7});
                        break;
                    case ast2ts.IdentUsage.define_fn:
                        locList.push({loc: node.loc,length: identLen,index:8});
                        break;
                    case ast2ts.IdentUsage.reference_builtin_fn:
                        locList.push({loc: node.loc,length: identLen,index:9});
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

export const SymbolKindStub = {
    Class: 5,
    Method: 6,
    Function: 12,
    Variable: 13,
    Constant: 14,
} as const;
export type SymbolKindStub = (typeof SymbolKindStub)[keyof typeof SymbolKindStub];

const identToSymbolKind = (node :ast2ts.Ident) :SymbolKindStub => {
    switch (node.usage) {
        case ast2ts.IdentUsage.define_variable:
        case ast2ts.IdentUsage.define_arg:
        case ast2ts.IdentUsage.define_field:
            return SymbolKindStub.Variable;
        case ast2ts.IdentUsage.define_const:
        case ast2ts.IdentUsage.define_enum_member:
            return SymbolKindStub.Constant;
        case ast2ts.IdentUsage.define_fn:
        case ast2ts.IdentUsage.define_cast_fn:
            if(ast2ts.isFunction(node.base)){
                if(node.base.belong == null) {
                    return SymbolKindStub.Function;
                } 
            }
            return SymbolKindStub.Method;
        case ast2ts.IdentUsage.define_format:
        case ast2ts.IdentUsage.define_enum:
        case ast2ts.IdentUsage.define_state:
                return SymbolKindStub.Class;
        default:
            return SymbolKindStub.Variable;
    }
}

export interface DocumentSymbolStub {
    name: string;
    kind: SymbolKindStub;
    range: RangeStub;
    selectionRange: RangeStub;
    children? :DocumentSymbolStub[];
}

export const CompletionItemKindStub = {
    Method: 2,
    Function: 3,
    Field: 5,
    Variable: 6,
    Class: 7,
    Enum: 13,
    Keyword: 14,
    EnumMember: 20,
    Constant: 21,
    Struct: 22,
    TypeParameter: 25,
} as const;
export type CompletionItemKindStub = (typeof CompletionItemKindStub)[keyof typeof CompletionItemKindStub];

export interface CompletionItemStub {
    label: string;
    kind: CompletionItemKindStub;
    detail?: string;
    documentation?: string;
}

const BGN_KEYWORDS = [
    "format", "if", "elif", "else", "match", "fn", "for", "enum",
    "input", "output", "config", "true", "false",
    "return", "break", "continue", "state",
];

// uN/iN/fN accept arbitrary bit widths (also with b/l endian prefix).
// These are hints for common widths only.
const BGN_BUILTIN_FIXED_TYPES = ["bool", "void"] as const;
const BGN_INT_COMMON_WIDTHS = [8, 16, 32, 64] as const;
const BGN_FLOAT_COMMON_WIDTHS = [16, 32, 64] as const;

const identUsageToCompletionKind = (usage :ast2ts.IdentUsage) :CompletionItemKindStub => {
    switch (usage) {
        case ast2ts.IdentUsage.define_format:
        case ast2ts.IdentUsage.define_state:
            return CompletionItemKindStub.Class;
        case ast2ts.IdentUsage.define_enum:
            return CompletionItemKindStub.Enum;
        case ast2ts.IdentUsage.define_enum_member:
            return CompletionItemKindStub.EnumMember;
        case ast2ts.IdentUsage.define_field:
            return CompletionItemKindStub.Field;
        case ast2ts.IdentUsage.define_variable:
        case ast2ts.IdentUsage.define_arg:
            return CompletionItemKindStub.Variable;
        case ast2ts.IdentUsage.define_const:
            return CompletionItemKindStub.Constant;
        case ast2ts.IdentUsage.define_fn:
            return CompletionItemKindStub.Function;
        case ast2ts.IdentUsage.define_cast_fn:
            return CompletionItemKindStub.Method;
        case ast2ts.IdentUsage.define_type_parameter:
            return CompletionItemKindStub.TypeParameter;
        default:
            return CompletionItemKindStub.Variable;
    }
};

const identDefineDetail = (ident :ast2ts.Ident) :string => {
    return `${ident.usage}: ${typeToString(ident.expr_type)}`;
};

const collectMembersFromStruct = (st :ast2ts.StructType, into :CompletionItemStub[], seen :Set<string>) => {
    for (const m of st.fields) {
        if (m.ident == null) {
            continue;
        }
        const name = m.ident.ident;
        if (seen.has(name)) {
            continue;
        }
        seen.add(name);
        into.push({
            label: name,
            kind: identUsageToCompletionKind(m.ident.usage),
            detail: identDefineDetail(m.ident),
        });
    }
};

const collectMembersFromType = (type :ast2ts.Type|null|undefined, into :CompletionItemStub[]) => {
    if (!type) {
        return;
    }
    if (ast2ts.isIdentType(type)) {
        collectMembersFromType(type.base, into);
        return;
    }
    if (ast2ts.isStructType(type)) {
        const seen = new Set<string>();
        collectMembersFromStruct(type, into, seen);
        if (ast2ts.isFormat(type.base)) {
            for (const f of type.base.cast_fns) {
                if (f.ident != null && !seen.has(f.ident.ident)) {
                    seen.add(f.ident.ident);
                    into.push({
                        label: f.ident.ident,
                        kind: CompletionItemKindStub.Method,
                        detail: `cast fn -> ${typeToString(f.return_type)}`,
                    });
                }
            }
        }
        return;
    }
    if (ast2ts.isStructUnionType(type)) {
        const seen = new Set<string>();
        for (const st of type.structs) {
            collectMembersFromStruct(st, into, seen);
        }
        return;
    }
    if (ast2ts.isArrayType(type)) {
        // Builtin: arrays expose `.length`. Element-type members aren't
        // accessible directly on the array itself.
        into.push({
            label: "length",
            kind: CompletionItemKindStub.Field,
            detail: "array length (builtin)",
        });
        return;
    }
    if (ast2ts.isEnumType(type)) {
        // A *value* of enum type. Members go through the type name (handled
        // separately in analyzeCompletion); on a value the builtin
        // `.is_defined` checks whether the value matches a declared member.
        into.push({
            label: "is_defined",
            kind: CompletionItemKindStub.Method,
            detail: "bool — true if the value matches a declared enum member (builtin)",
        });
        return;
    }
};

const findIdentByName = (prevNode :ast2ts.ParseResult, name :string) :ast2ts.Ident|null => {
    for (const sc of prevNode.scope) {
        for (const id of sc.ident) {
            if (id.ident !== name) {
                continue;
            }
            if (id.usage === ast2ts.IdentUsage.reference || id.usage === ast2ts.IdentUsage.unknown) {
                continue;
            }
            return id;
        }
    }
    for (const sc of prevNode.scope) {
        for (const id of sc.ident) {
            if (id.ident === name && id.expr_type != null) {
                return id;
            }
        }
    }
    return null;
};

const isDefiningUsage = (u :ast2ts.IdentUsage) :boolean => {
    switch (u) {
        case ast2ts.IdentUsage.define_variable:
        case ast2ts.IdentUsage.define_const:
        case ast2ts.IdentUsage.define_field:
        case ast2ts.IdentUsage.define_format:
        case ast2ts.IdentUsage.define_state:
        case ast2ts.IdentUsage.define_enum:
        case ast2ts.IdentUsage.define_enum_member:
        case ast2ts.IdentUsage.define_fn:
        case ast2ts.IdentUsage.define_cast_fn:
        case ast2ts.IdentUsage.define_arg:
        case ast2ts.IdentUsage.define_type_parameter:
            return true;
        default:
            return false;
    }
};

// Find the innermost scope whose loc contains `pos`. Scope.loc covers the
// full body extent (set in src2json's parse_indent_block), so a position
// inside the body matches the scope correctly. Ties (e.g. sibling scopes
// sharing the same owner) are broken by depth — deeper = later sibling.
const findEnclosingScope = (prevNode :ast2ts.ParseResult, pos :number) :ast2ts.Scope|null => {
    let best :ast2ts.Scope|null = null;
    let bestSize = Number.POSITIVE_INFINITY;
    let bestDepth = -1;
    const depthOf = (sc :ast2ts.Scope) => {
        let d = 0;
        let cur :ast2ts.Scope|null = sc;
        while (cur !== null) {
            d++;
            cur = cur.prev;
        }
        return d;
    };
    for (const sc of prevNode.scope) {
        const loc = sc.loc;
        if (loc.file !== 1) {
            continue;
        }
        if (loc.pos.begin <= pos && pos <= loc.pos.end) {
            const size = loc.pos.end - loc.pos.begin;
            if (size < bestSize) {
                best = sc;
                bestSize = size;
                bestDepth = depthOf(sc);
                continue;
            }
            if (size === bestSize) {
                const d = depthOf(sc);
                if (d > bestDepth) {
                    best = sc;
                    bestDepth = d;
                }
            }
        }
    }
    return best;
};

const pushIdentItem = (id :ast2ts.Ident, into :CompletionItemStub[], seen :Set<string>) => {
    if (!isDefiningUsage(id.usage)) {
        return;
    }
    const key = `${id.ident}\0${id.usage}`;
    if (seen.has(key)) {
        return;
    }
    seen.add(key);
    into.push({
        label: id.ident,
        kind: identUsageToCompletionKind(id.usage),
        detail: identDefineDetail(id),
    });
};

// Mirror C++ Scope::lookup_backward: walk prev (parent / earlier sibling) chain.
const collectVisibleIdents = (start :ast2ts.Scope, into :CompletionItemStub[], seen :Set<string>) => {
    let sc :ast2ts.Scope|null = start;
    const visited = new Set<ast2ts.Scope>();
    while (sc !== null && !visited.has(sc)) {
        visited.add(sc);
        for (const id of sc.ident) {
            pushIdentItem(id, into, seen);
        }
        sc = sc.prev;
    }
};

// global_scope walk: include all top-level definitions (format/enum/state/fn)
// regardless of where the cursor sits, since they're file-global.
const collectGlobalDefinitions = (globalScope :ast2ts.Scope|null, into :CompletionItemStub[], seen :Set<string>) => {
    let sc :ast2ts.Scope|null = globalScope;
    while (sc !== null) {
        for (const id of sc.ident) {
            pushIdentItem(id, into, seen);
        }
        sc = sc.next;
    }
};

export const analyzeCompletion = (
    prevNode :ast2ts.ParseResult|null,
    pos :number,
    memberOfName? :string,
) :CompletionItemStub[] => {
    const items :CompletionItemStub[] = [];
    if (memberOfName !== undefined && memberOfName !== "" && prevNode !== null) {
        const target = findIdentByName(prevNode, memberOfName);
        if (target !== null) {
            // enum type name (e.g. `Color.`): list declared enum members.
            // The value-side path (variable of enum type) is handled via
            // collectMembersFromType's EnumType branch (.is_defined).
            if (target.usage === ast2ts.IdentUsage.define_enum && ast2ts.isEnum(target.base)) {
                for (const em of target.base.members) {
                    if (em.ident !== null) {
                        items.push({
                            label: em.ident.ident,
                            kind: CompletionItemKindStub.EnumMember,
                            detail: identDefineDetail(em.ident),
                        });
                    }
                }
                return items;
            }
            collectMembersFromType(target.expr_type, items);
        }
        return items;
    }

    for (const kw of BGN_KEYWORDS) {
        items.push({ label: kw, kind: CompletionItemKindStub.Keyword });
    }
    for (const t of BGN_BUILTIN_FIXED_TYPES) {
        items.push({ label: t, kind: CompletionItemKindStub.Struct, detail: "builtin type" });
    }
    const widthHint = "common width (any uN/iN allowed)";
    for (const n of BGN_INT_COMMON_WIDTHS) {
        items.push({ label: `u${n}`, kind: CompletionItemKindStub.Struct, detail: widthHint });
        items.push({ label: `i${n}`, kind: CompletionItemKindStub.Struct, detail: widthHint });
    }
    const floatHint = "common width (any fN allowed)";
    for (const n of BGN_FLOAT_COMMON_WIDTHS) {
        items.push({ label: `f${n}`, kind: CompletionItemKindStub.Struct, detail: floatHint });
    }

    if (prevNode === null) {
        return items;
    }

    const seen = new Set<string>();
    const enclosing = findEnclosingScope(prevNode, pos);
    if (enclosing !== null) {
        collectVisibleIdents(enclosing, items, seen);
    }
    // Always include top-level format/enum/state/fn idents.
    collectGlobalDefinitions(prevNode.root.global_scope, items, seen);
    return items;
};

export const analyzeSymbols = (scope :ast2ts.Scope,depth :number = 0) :DocumentSymbolStub[] => {
    var symbols :DocumentSymbolStub[] = [];
    let preSym :DocumentSymbolStub | null = null;
    for(let sc=scope as ast2ts.Scope | null;sc != null;sc = sc.next) {
        for(const s of sc.ident) {
            if(s.usage === ast2ts.IdentUsage.reference || s.usage == ast2ts.IdentUsage.reference_member || s.usage == ast2ts.IdentUsage.reference_member_type ||
                s.usage === ast2ts.IdentUsage.reference_type || s.usage === ast2ts.IdentUsage.reference_builtin_fn ||
                s.usage === ast2ts.IdentUsage.unknown
            ) {
                continue; // skip references
            }
            let sym : DocumentSymbolStub = {
                name: s.ident,
                kind: identToSymbolKind(s),
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
        if(sc.branch != null) {
            if(preSym != null) {
                preSym.children = analyzeSymbols(sc.branch,depth+1);
            }
            else {
                symbols = symbols.concat(analyzeSymbols(sc.branch,depth+1));
            }
        }
    }
    return symbols;
}


}

export {analyze}
