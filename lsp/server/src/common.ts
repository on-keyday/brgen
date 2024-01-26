import {ast2ts} from "ast2ts";

const unwrapType = (type :ast2ts.Type|null|undefined) :string => {
    if(!type){
        return "unknown";
    }
    if(ast2ts.isIdentType(type)){
       return unwrapType(type.base); 
    }
    return type.node_type;
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

const makeHover = (ident :string,role :string) => {
    return {
        contents: {
            kind: "markdown",
            value: `### ${ident}\n\n${role}`,
        },
    };
}

const analyzeHover = async (prevNode :ast2ts.Program, pos :number) =>{
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


export {analyzeHover}