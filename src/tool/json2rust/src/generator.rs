use anyhow::{anyhow, Result};
use ast2rust::ast::{GenerateMapFile, StructType, StructUnionType, UnionType};
use ast2rust::eval::{get_int_type, get_type_prim, is_any_range, is_non_primitive_array, is_primitive_array, is_struct_type, lookup_base, topological_sort_format, Stringer};
use ast2rust::{ast, PtrKey, PtrUnwrapError};
use ast2rust_macro::{ptr, ptr_null};
use std::cell::RefCell;
use std::collections::{HashMap, HashSet};
use std::rc::Rc;

type SharedPtr<T> = Rc<RefCell<T>>;

struct AnonymousStruct {
    field: SharedPtr<ast::Field>,
    base: String,
    name: String,
}

struct AnonymousStructUnion {
    field: SharedPtr<ast::Field>,
    name: String,
}

struct BitFields {
    fields: Vec<SharedPtr<ast::Field>>,
    int_ty :ast::Type,
    name: String,
}

pub struct Generator<W: std::io::Write> {
    w: Writer<W>,
    seq: usize,
    encode: bool,

    pub map_file: GenerateMapFile,

    structs: HashMap<PtrKey<StructType>, AnonymousStruct>,
    struct_unions: HashMap<PtrKey<StructUnionType>, AnonymousStructUnion>,
    s: Stringer,

    bit_fields: HashMap<PtrKey<ast::Field>, BitFields>,
    bit_field_parts: HashSet<PtrKey<ast::Field>>,
}

pub struct Writer<W: std::io::Write> {
    pub writer: W,
    indent: RefCell<usize>,

    should_indent: bool,

    pub prefix: String,
}

struct IndentScope<'a> {
    indent: &'a RefCell<usize>,
}

impl Drop for IndentScope<'_> {
    fn drop(&mut self) {
        *self.indent.borrow_mut() -= 1;
    }
}

impl<'a, W: std::io::Write> Writer<W> {
    pub fn new(w: W) -> Self {
        Self {
            writer: w,
            indent: RefCell::new(0),
            should_indent: false,
            prefix: String::new(),
        }
    }

    pub fn flush(&mut self) -> Result<()> {
        self.writer.flush()?;
        Ok(())
    }

    pub fn get_mut_writer(&mut self) -> &mut W {
        &mut self.writer
    }

    fn enter_indent_scope(&self) -> IndentScope<'a> {
        *self.indent.borrow_mut() += 1;
        IndentScope {
            // to escape from the lifetime checker,
            // we need to cast the reference to a raw pointer
            // and then back to a reference.
            // so we can create a reference with a new lifetime.
            indent: unsafe { &*(&self.indent as *const _) },
        }
    }

    fn write_indent(&mut self) -> Result<()> {
        for _ in 0..*self.indent.borrow() {
            self.writer.write(b"    ")?;
        }
        Ok(())
    }

    fn write(&mut self, s: &str) -> Result<()> {
        self.write_if_should_indent()?;
        self.writer.write(s.as_bytes())?;
        Ok(())
    }

    fn write_if_should_indent(&mut self) -> Result<()> {
        if self.should_indent {
            self.write_indent()?;
            self.should_indent = false;
        }
        Ok(())
    }

    fn writeln(&mut self, s: &str) -> Result<()> {
        self.write(s)?;
        self.write("\n")?;
        self.should_indent = true;
        Ok(())
    }
}

impl<W: std::io::Write> Generator<W> {
    pub fn new(w: W) -> Self {
        Self {
            w: Writer::new(w),
            seq: 0,
            encode: false,
            structs: HashMap::new(),
            struct_unions: HashMap::new(),
            map_file: GenerateMapFile {
                structs: Vec::new(),
                line_map: Vec::new(),
            },
            s: Stringer::new("self".into()),
            bit_fields: HashMap::new(),
            bit_field_parts: HashSet::new(),
        }
    }

    fn get_seq(&mut self) -> usize {
        let seq = self.seq;
        self.seq += 1;
        seq
    }

    pub fn get_mut_writer(&mut self) -> &mut W {
        self.w.get_mut_writer()
    }

    pub fn expr_to_string(&self, x: &ast::Expr) -> Result<String> {
        return self.s.to_string(x).map_err(|x| anyhow!("{:?}", x));
    }

   

 

    pub fn gen_anonymous_union<W1: std::io::Write>(
        &mut self,
        w: &mut Writer<W1>,
        x: &SharedPtr<StructUnionType>,
        field: &SharedPtr<ast::Field>,
    ) -> Result<String> {
        let name = format!("AnonymousStructUnion{}", self.get_seq());
        self.struct_unions.insert(
            PtrKey::new(x),
            AnonymousStructUnion {
                field: field.clone(),
                name: name.clone(),
            },
        );
        let mut v = Vec::new();
        let field_name = ptr_null!(field.ident.ident);
        let prefix = &w.prefix;
        let union_field_access = format!("{prefix}.{field_name}");
        for structs in ptr_null!(x.structs).iter() {
            let name_s = format!("AnonymousStruct{}", self.get_seq());
            let mut tmp_w = Writer::new(Vec::new());
    
            tmp_w.prefix = format!(
                "(match $MUT {union_field_access} {{ {name}::{name_s}(x) => x, x => $ERROR }})"
            );
            tmp_w.writeln("#[derive(Default,Debug,Clone,PartialEq)]")?;
            tmp_w.write("pub struct ")?;
            tmp_w.write(&name_s)?;
            tmp_w.writeln(" {")?;
            self.write_struct_type(&mut tmp_w, structs.clone())?;
            tmp_w.writeln("}")?;
            self.w.write(&String::from_utf8(tmp_w.writer)?)?;
            self.structs.insert(
                PtrKey::new(structs),
                AnonymousStruct {
                    field: field.clone(),
                    base: name.clone(),
                    name: name_s.clone(),
                },
            );
            v.push(name_s);
        }
        let mut tmp_w = Writer::new(Vec::new());
        tmp_w.writeln("#[derive(Default,Debug,Clone,PartialEq)]")?;
        tmp_w.write("pub enum ")?;
        tmp_w.write(&name)?;
        tmp_w.writeln(" {")?;
        tmp_w.writeln("#[default]")?;
        tmp_w.writeln("None,")?;
        for name in v.iter() {
            tmp_w.write(&name)?;
            tmp_w.write("(")?;
            tmp_w.write(&name)?;
            tmp_w.writeln("),")?;
        }
        tmp_w.writeln("}")?;

        let union_fields = ptr_null!(x.union_fields);
        let memb = ptr!(field.belong)
            .upgrade()
            .ok_or_else(|| anyhow!("member unwrap error"))?;
        for field in union_fields.iter() {
            let typ: SharedPtr<UnionType> = ptr!(field->field_type)
                .try_into()
                .map_err(|x| anyhow!("{:?}", x))?;
            let cond0 = ptr_null!(typ.cond);
            let cond0 = if let Some(x) = cond0 {
                x.upgrade()
            } else {
                None
            };
            let c = ptr_null!(typ.common_type);
            if let Some(c) = c {
                let is_struct_ty = is_struct_type(&c) || is_non_primitive_array(&c);
                let c_type = self.get_type(&c)?;
                let impl_target = memb.get_ident().ok_or_else(|| anyhow!("ident unwrap error"))?;
                let impl_target = ptr_null!(impl_target.ident);
                tmp_w.writeln(&format!("impl {impl_target} {{"))?;
                {
                    let _scope = tmp_w.enter_indent_scope();
                    let escaped = Self::escape_keyword(ptr_null!(field->ident.ident));
                    self.s.add_map(&ptr!(field->ident), &format!("{prefix}.{escaped}().unwrap()"));
                    self.s.error_string = "{_ = x; return None}".to_string();
                    self.s.mutator = "&".to_string();
                    let as_ref = if is_struct_ty  { "&" } else { "" };
                    tmp_w.writeln(&format!("pub fn {}(&self) -> Option<{}{}> {{", escaped,as_ref, c_type))?;
                    {
                        let _scope = tmp_w.enter_indent_scope();

                        for cand in ptr_null!(typ.candidates) {
                            let cond = ptr!(cand.cond).upgrade().ok_or_else(|| anyhow!("cond unwrap error"))?;
                            let cond = if let Some(cond0) = &cond0 {
                                self.s
                                    .eval_ephemeral_binary_op(ast::BinaryOp::Equal, &cond, cond0)
                                    .map_err(|x| anyhow!("{:?}", x))?
                            } else {
                                self.expr_to_string(&cond)?
                            };

                            tmp_w.writeln(&format!("if {cond} {{ "))?;
                            {
                                let _scope = tmp_w.enter_indent_scope();
                                if let Some(field) = ptr_null!(cand.field) {
                                    if let Some(field) = field.upgrade() {
                                        
                                        let ident =
                                            self.expr_to_string(&ptr!(field.ident).into())?;
                                        tmp_w.writeln(&format!("return Some({}{}{});",as_ref, ident,if !is_struct_ty{
                                            " as _"
                                        } else {""}))?;
                                    } else {
                                        tmp_w.writeln("return None")?;
                                    }
                                } else {
                                    tmp_w.writeln("return None")?;
                                }
                            }
                            tmp_w.writeln("}")?;
                        }

                        tmp_w.writeln("None")?;
                    }
                    tmp_w.writeln("}")?;

                    if is_struct_ty {
                        tmp_w.writeln(&format!("pub fn {}_mut<'a>(&'a mut self) -> Option<&'a mut {}> {{", escaped, c_type))?;
                        tmp_w.writeln(&format!("let x = self.{escaped}();"))?;
                        tmp_w.writeln(&format!("match x {{"))?;
                        tmp_w.writeln("// SAFETY: this reference is derived from `self`")?;
                        tmp_w.writeln(&format!("Some(x) => Some(unsafe {{ &mut *(x as *const _ as *mut _) }}),"))?;
                        tmp_w.writeln(&format!("None => None,"))?;
                        tmp_w.writeln(&format!("}}"))?;
                        tmp_w.writeln("}")?;
                    }

                    self.s.error_string = format!(
                        "return Err(Error::UnwrapError(format!(\"unwrapping {{:?}} failed\",x))),"
                    );
                    self.s.mutator = "&mut ".to_string();
                    tmp_w.writeln(&format!(
                        "pub fn set_{}(&mut self, x: {}) -> Result<(),Error> {{",
                        ptr_null!(field->ident.ident),
                        c_type
                    ))?;
                    {
                        let _scope = tmp_w.enter_indent_scope();

                        for cand in ptr_null!(typ.candidates) {
                            let cond = ptr!(cand.cond).upgrade().ok_or_else(|| anyhow!("cond unwrap error"))?;
                            let cond = if let Some(cond0) = &cond0 {
                                self.s
                                    .eval_ephemeral_binary_op(ast::BinaryOp::Equal, &cond, cond0)
                                    .map_err(|x| anyhow!("{:?}", x))?
                            } else {
                                self.expr_to_string(&cond)?
                            };

                            tmp_w.writeln(&format!("if {cond} {{ "))?;
                            {
                                let _scope = tmp_w.enter_indent_scope();
                                if let Some(field) = ptr_null!(cand.field) {
                                    if let Some(field) = field.upgrade() {
                                        let belong_struct =  ptr!(field.belong_struct).upgrade().ok_or_else(|| anyhow!("belong unwrap error"))?;
                                        let anonymous_field = self.structs.get(&PtrKey::new(&belong_struct)).ok_or_else(|| anyhow!("belong_struct unwrap error"))?;                                        
                                        let union_field_access = self.s.replace_with_this(&self.s.self_, &union_field_access).map_err(|x| anyhow!("{:?}", x))?;
                                        self.s.error_string =format!("{{ _ = x;{union_field_access} = {name}::{name_s}(Default::default()); match &mut {union_field_access} {{ {name}::{name_s}(x) => x, _ => unreachable!() }}}}", union_field_access = union_field_access, name = name, name_s = anonymous_field.name);
                                        if !is_struct_ty {
                                            tmp_w.writeln(&format!("let x = x.try_into().map_err(|_| Error::SetError(\"cannot set value\"))?;"))?;
                                        }
                                        let ident =
                                            self.expr_to_string(&ptr!(field.ident).into())?;
                                        tmp_w.writeln(&format!("{} = x;", ident))?;
                                        tmp_w.writeln("return Ok(())")?;
                                    } else {
                                        tmp_w.writeln("return Err(Error::SetError(\"cannot set value\"))")?;
                                    }
                                } else {
                                    tmp_w.writeln("return Err(Error::SetError(\"cannot set value\"))")?;
                                }
                            }
                            tmp_w.writeln("}")?;
                        }
                        tmp_w.write(
                            "Err(Error::SetError(\"not matching field\"))",
                        )?;
                    }
                    tmp_w.writeln("}")?;
                }
                tmp_w.writeln("}")?;
            }
        }

        self.w.write(std::str::from_utf8(&tmp_w.writer)?)?;
        Ok(name)
    }

    pub fn get_type(&mut self, typ: &ast::Type) -> Result<String> {
        match typ {
            ast::Type::StructUnionType(x) => {
                if let Some(x) = self.struct_unions.get(&PtrKey::new(x)) {
                    return Ok(x.name.clone());
                } else {
                    return Err(anyhow!("unsupported type"));
                }
            }
            _ => {
               get_type_prim(typ).map_err(|x| anyhow!("{:?}", x))
            }
        }
    }

    pub fn write_bit_field<W1: std::io::Write>(
        &mut self,
        w: &mut Writer<W1>,
        bit_fields: &Vec<SharedPtr<ast::Field>>,
    ) -> Result<()> {
        let sum: u64 = bit_fields
            .iter()
            .try_fold(0 as u64, |acc, x| {
                Some(acc + x.borrow().field_type.as_ref()?.get_bit_size()?)
            })
            .ok_or_else(|| anyhow!("sum error"))?;
        if sum % 8 != 0 {
            return Err(anyhow!("not aligned error"));
        }
        let sum = sum;
        let ident = format!("bit_field{}", self.get_seq());
        w.writeln(&format!("{ident}: u{sum},"))?;
        let mut tmp_w = Writer::new(Vec::new());
        let prefix = &w.prefix;
        let bit_field_access = self
            .s
            .replace_with_this(&self.s.self_, &format!("{prefix}.{ident}"))
            .map_err(|x| anyhow!("{:?}", x))?;
        let field = &bit_fields[0];
        let belong_ident = ptr!(field.belong)
            .upgrade()
            .ok_or_else(|| anyhow!("weak unwrap error"))?
            .get_ident()
            .ok_or_else(|| anyhow!("belong ident error"))?;
        let belong_ident = ptr_null!(belong_ident.ident);
        tmp_w.writeln(&format!("impl {belong_ident} {{"))?;
        let mut shift_sum =0;
        bit_fields.iter().enumerate().try_for_each(|(i,field)| -> Result<()> {
            let bit_size = ptr!(field.field_type)
                .get_bit_size()
                .ok_or_else(|| anyhow!("bit fields error"))?;
            let ident = ptr_null!(field.ident.ident);
            let escaped = Self::escape_keyword(ident.clone());
            tmp_w.writeln("#[inline]")?;
            let mut is_ident_type = false;
            let pure_int_type =get_int_type(bit_size, false, false).map_err(|x| anyhow!("{:?}", x))?;
            let int_type = if let ast::Type::IdentType(t) = ptr!(field.field_type) {
                is_ident_type = true;
                ptr_null!(t.ident.ident)
            }else if bit_size == 1 {
                "bool".to_string()
            } else { 
                pure_int_type.clone()
            };
            tmp_w.writeln(&format!("pub fn {escaped}(&self) -> {int_type} {{"))?;
            shift_sum += bit_size;
            let shift = sum - shift_sum;
            let mask = (1 << bit_size) - 1;
            let suffix =
            if is_ident_type {
                ".into()".to_string()
            }else            if int_type == "bool" {
                "!= 0".to_string()
            } else {
                format!(
                    "as {int_type}"
                )
            };
            tmp_w.writeln(&format!(
                "(({bit_field_access} >> {shift}) & {mask}) {suffix}"
            ))?;
            tmp_w.writeln("}")?;
            if int_type == "bool" {
                self.s.add_map(&ptr!(field.ident), &format!("if {prefix}.{escaped}() {{ 1 }} else {{ 0 }}"));
            } else {
                self.s.add_map(&ptr!(field.ident), &format!("{prefix}.{escaped}()"));
            }
            tmp_w.writeln("#[inline]")?;
            tmp_w.writeln(&format!("pub fn set_{ident}(&mut self, x: {int_type}) {{"))?;
            if is_ident_type {
                tmp_w.writeln(&format!(
                    "let x :{pure_int_type} = x.into();",
                ))?;
                if int_type == "bool" {
                      tmp_w.writeln("let x = x != 0")?;
                } 
            }
            let x =  if int_type == "bool" {
                "if x { 1 } else { 0 }"
            } else {
                "x"
            };
            tmp_w.writeln(&format!(
                "{bit_field_access} = ({bit_field_access} & !({mask} << {shift})) | (({x} & {mask}) << {shift});"
            ))?;
            tmp_w.writeln("}")?;
            if i != bit_fields.len() - 1 {
                self.bit_field_parts.insert(PtrKey::new(field));
            } else {
                self.bit_fields.insert(
                    PtrKey::new(field),
                    BitFields {
                        fields: bit_fields.clone(),
                        int_ty :ast::Type::IntType(Rc::new(RefCell::new(ast::IntType{
                            loc: field.borrow().loc.clone(),
                            bit_size: Some(sum),
                            is_signed: false,
                            is_common_supported: true,
                            endian: ast::Endian::Unspec,
                            is_explicit: false,
                            non_dynamic_allocation: true,
                            bit_alignment: ast::BitAlignment::ByteAligned,
                        }))),
                        name: bit_field_access.clone(),
                    },
                );
            }
            Ok(())
        })?;
        tmp_w.writeln("}")?;
        self.w.write(std::str::from_utf8(&tmp_w.writer)?)?;
        Ok(())
    }

    pub fn escape_keyword(s: String) -> String {
        match s.as_str() {
            "type" => "r#type".to_string(),
            "move" => "r#move".to_string(),
            _ => s,
        }
    }

    pub fn write_field<W1: std::io::Write>(
        &mut self,
        w: &mut Writer<W1>,
        field: &SharedPtr<ast::Field>,
        bit_fields: &mut Vec<SharedPtr<ast::Field>>,
    ) -> Result<()> {
        if ptr_null!(field.bit_alignment) != ptr_null!(field.eventual_bit_alignment) {
            bit_fields.push(field.clone());
            return Ok(());
        }
        if bit_fields.len() > 0 {
            bit_fields.push(field.clone());
            self.write_bit_field(w, bit_fields)?;
            bit_fields.clear();
            return Ok(());
        }
        if let ast::Type::UnionType(_) = ptr!(field.field_type) {
            return Ok(());
        }
        if field.borrow().ident.is_none() {
            let some = Some(Rc::new(RefCell::new(ast::Ident {
                loc: field.borrow().loc.clone(),
                expr_type: ptr_null!(field.field_type),
                ident: format!("hidden_field_{}", self.get_seq()),
                base: Some(ast::NodeWeak::Field(Rc::downgrade(&field))),
                constant_level: ast::ConstantLevel::Variable,
                usage: ast::IdentUsage::DefineField,
                scope: None,
            })));
            field.borrow_mut().ident = some;
        }
        let prefix = &w.prefix;
        let ident = ptr_null!(field.ident.ident);
        let ident = Self::escape_keyword(ident);
        self.s
            .add_map(&ptr!(field.ident), &format!("{prefix}.{ident}"));
        if let ast::Type::StructUnionType(x) = ptr!(field.field_type) {
            self.gen_anonymous_union(w, &x, field)?;
        }
        w.write("pub ")?;
        w.write(&format!("{}: ", ident))?;
        let typ = self.get_type(&ptr!(field.field_type))?;
        w.writeln(&format!("{},", typ))?;

        Ok(())
    }

    pub fn write_struct_type<W1: std::io::Write>(
        &mut self,
        w: &mut Writer<W1>,
        ty: SharedPtr<ast::StructType>,
    ) -> Result<()> {
        let ty = ty.borrow();
        let mut bit_fields = Vec::new();
        for field in ty.fields.iter() {
            if let ast::Member::Field(field) = field {
                self.write_field(w, field, &mut bit_fields)?;
            }
        }
        Ok(())
    }

    pub fn write_encode_type<W1: std::io::Write>(
        &mut self,
        w: &mut Writer<W1>,
        field: &ast::Field,
        typ: ast::Type,
        ident: &str,
        err_ident: &str,
    ) -> Result<()> {
        match typ {
            ast::Type::IntType(ity) => {
                if ptr_null!(ity.is_common_supported) {
                    match ptr_null!(ity.endian) {
                        ast::Endian::Big | ast::Endian::Unspec => {
                            w.writeln(&format!("w.write_all(&{}.to_be_bytes()).map_err(|e| Error::Io(\"{err_ident}\",e))?;", ident))?;
                        }
                        ast::Endian::Little => {
                            w.writeln(&format!("w.write_all(&{}.to_le_bytes()).map_err(|e| Error::Io(\"{err_ident}\",e))?;", ident))?;
                        }
                    }
                }
                return Ok(());
            }
            ast::Type::FloatType(fty) => {
                let bit_size = ptr!(fty.bit_size);
                let le_or_be = match ptr_null!(fty.endian) {
                    ast::Endian::Big | ast::Endian::Unspec => "be",
                    ast::Endian::Little => "le",
                };
                match bit_size {
                    16 => {
                        w.writeln(&format!(
                            "w.write_all(&{ident}.to_bits().to_{le_or_be}_bytes()).map_err(|e| Error::Io(\"{err_ident}\",e))?;"
                        ))?;
                    }
                    32 => {
                        w.writeln(&format!(
                            "w.write_all(&{ident}.to_bits().to_{le_or_be}_bytes()).map_err(|e| Error::Io(\"{err_ident}\",e))?;"
                        ))?;
                    }
                    64 => {
                        w.writeln(&format!(
                            "w.write_all(&{ident}.to_bits().to_{le_or_be}_bytes()).map_err(|e| Error::Io(\"{err_ident}\",e))?;"
                        ))?;
                    }
                    _ => {
                        return Err(anyhow!("unsupported float size"));
                    }
                }
                return Ok(());
            }
            ast::Type::IdentType(t) => {
                let base = ptr!(t.base)
                    .try_into()
                    .map_err(|x: ast::Error| anyhow!("{:?}", x))?;
                self.write_encode_type(w, field, base, ident,err_ident)?;
                return Ok(());
            }
            ast::Type::StructType(_) => {
                w.writeln(&format!("{}.encode(w).map_err(|e| Error::Nested(\"{err_ident}\",Box::new(e)))?;", ident))?;
                return Ok(());
            }
            ast::Type::EnumType(t) => {
                let base = ptr!(t.base->base_type);
                self.write_encode_type(w, field, base, ident,err_ident)?;
                return Ok(());
            }
            ast::Type::ArrayType(t) => {
                if is_primitive_array(&t) {
                    if ptr_null!(t.is_bytes) {
                        w.writeln(&format!("w.write_all(&{}).map_err(|e| Error::Io(\"{err_ident}\",e))?;", ident))?;
                    } else {
                        w.writeln(&format!("for i in 0..{} {{", ptr!(t.length_value)))?;
                        {
                            let _scope = w.enter_indent_scope();
                            self.write_encode_type(
                                w,
                                field,
                                ptr!(t.element_type),
                                &format!("{}[i]", ident),
                                &format!("{}[i]", err_ident),
                            )?;
                        }
                        w.writeln("}")?;
                    }
                } else {
                    let len = match ptr_null!(t.length_value) {
                        None => self.expr_to_string(&ptr!(t.length))?,
                        Some(x) => x.to_string(),
                    };
                    w.writeln(&format!("if ({}) as usize != {}.len() {{", len, ident))?;
                    {
                        let _scope = w.enter_indent_scope();
                        let escaped_len = len.escape_default().to_string();
                        let escaped_ident = ident.escape_default().to_string();
                        w.writeln(&format!(
                            "return Err(Error::LengthError(\"length {escaped_len} and {escaped_ident}.len() not equal\"))"
                        ))?;
                    }
                    w.writeln("}")?;
                    if ptr_null!(t.is_bytes) {
                        w.writeln(&format!("w.write_all(&{}).map_err(|e| Error::Io(\"{err_ident}\",e))?;", ident))?;
                    } else {
                        w.writeln(&format!("for i in 0..{}.len() {{", ident))?;
                        {
                            let _scope = w.enter_indent_scope();
                            self.write_encode_type(
                                w,
                                field,
                                ptr!(t.element_type),
                                &format!("{}[i]", ident),
                                &format!("{}[i]", err_ident),
                            )?;
                        }
                        w.writeln("}")?;
                    }
                }
                return Ok(());
            }
            _ => {}
        }
        Err(anyhow!("unsupported encode type"))
    }

    pub fn write_decode_type<W1: std::io::Write>(
        &mut self,
        w: &mut Writer<W1>,
        field: &ast::Field,
        typ: ast::Type,
        ident: &str,
        err_ident: &str,
    ) -> Result<()> {
        match typ {
            ast::Type::IntType(ity) => {
                if ptr_null!(ity.is_common_supported) {
                    let tmp = format!("tmp{}", self.get_seq());
                    let len = ptr!(ity.bit_size) / 8;
                    w.writeln(&format!("let mut {} = [0; {}];", tmp, len))?;
                    w.writeln(&format!("r.read_exact(&mut {}).map_err(|e| Error::Io(\"{err_ident}\",e))?;", tmp))?;
                    match ptr_null!(ity.endian) {
                        ast::Endian::Big | ast::Endian::Unspec => {
                            w.writeln(&format!(
                                "{} = u{}::from_be_bytes({});",
                                ident,
                                ptr!(ity.bit_size),
                                tmp,
                            ))?;
                        }
                        ast::Endian::Little => {
                            w.writeln(&format!(
                                "{} = u{}::from_le_bytes({});",
                                ident,
                                ptr!(ity.bit_size),
                                tmp,
                            ))?;
                        }
                    }
                }
                return Ok(());
            }
            ast::Type::FloatType(t) => {
                let bit_size = ptr!(t.bit_size);
                let le_or_be = match ptr_null!(t.endian) {
                    ast::Endian::Big | ast::Endian::Unspec => "be",
                    ast::Endian::Little => "le",
                };
                let tmp = format!("tmp{}", self.get_seq());
                match bit_size {
                    16 => {
                        w.writeln(&format!("let mut {} = [0; 2];", tmp))?;
                        w.writeln(&format!("r.read_exact(&mut {}).map_err(|e| Error::Io(\"{err_ident}\",e))?;", tmp))?;
                        w.writeln(&format!(
                            "{ident} = f16::from_bits(u16::from_{le_or_be}_bytes({tmp}));"
                        ))?;
                    }
                    32 => {
                        w.writeln(&format!("let mut {} = [0; 4];", tmp))?;
                        w.writeln(&format!("r.read_exact(&mut {}).map_err(|e| Error::Io(\"{err_ident}\",e))?;", tmp))?;
                        w.writeln(&format!(
                            "{ident} = f32::from_bits(u32::from_{le_or_be}_bytes({tmp}));"
                        ))?;
                    }
                    64 => {
                        w.writeln(&format!("let mut {} = [0; 8];", tmp))?;
                        w.writeln(&format!("r.read_exact(&mut {}).map_err(|e| Error::Io(\"{err_ident}\",e))?;", tmp))?;
                        w.writeln(&format!(
                            "{ident} = f64::from_bits(u64::from_{le_or_be}_bytes({tmp}));"
                        ))?;
                    }
                    _ => {
                        return Err(anyhow!("unsupported float size"));
                    }
                }
                return Ok(());
            }
            ast::Type::IdentType(t) => {
                let base = ptr!(t.base)
                    .try_into()
                    .map_err(|x: ast::Error| anyhow!("{:?}", x))?;
                self.write_decode_type(w, field, base, ident,err_ident)?;
                return Ok(());
            }
            ast::Type::EnumType(t) => {
                let base = ptr!(t.base->base_type);
                let tmp = format!("tmp{}", self.get_seq());
                w.writeln(&format!("let mut {} = Default::default();", tmp))?;
                self.write_decode_type(w, field, base, &tmp,err_ident)?;
                w.writeln(&format!("{} = {}.into();", ident, tmp))?;
                return Ok(());
            }
            ast::Type::StructType(_) => {
                w.writeln(&format!("{ident}.decode_impl(r).map_err(|e| Error::Nested(\"{err_ident}\",Box::new(e)))?;"))?;
                return Ok(());
            }
            ast::Type::ArrayType(t) => {
                if is_primitive_array(&t) {
                    if ptr_null!(t.is_bytes) {
                        w.writeln(&format!("r.read_exact(&mut {}).map_err(|e| Error::Io(\"{err_ident}\",e))?;", ident))?;
                    } else {
                        let len = ptr!(t.length_value);
                        w.writeln(&format!("{} = [0; {}];", ident, len))?;
                        w.writeln(&format!("for i in 0..{} {{", len))?;
                        {
                            let _scope = w.enter_indent_scope();
                            self.write_decode_type(
                                w,
                                field,
                                ptr!(t.element_type),
                                &format!("{}[i]", ident),
                                &format!("{}[i]", err_ident),
                            )?;
                        }
                        w.writeln("}")?;
                    }
                } else {
                    let len = match ptr_null!(t.length_value) {
                        None => {
                            let cur_mutator = self.s.mutator.clone();
                            self.s.mutator = "&".to_string();
                            let len = self.expr_to_string(&ptr!(t.length))?;
                            self.s.mutator = cur_mutator;
                            len
                        },
                        Some(x) => x.to_string(),
                    };                 
                    if ptr_null!(t.is_bytes) {
                        w.writeln(&format!("{ident}.resize({len} as usize,0);"))?;
                        w.writeln(&format!("r.read_exact(&mut {}).map_err(|e| Error::Io(\"{err_ident}\",e))?;", ident))?;
                    } else {
                        w.writeln(&format!("{} = Vec::new();", ident))?;
                        w.writeln(&format!("for _ in 0..{} {{", len))?;
                        {
                            let _scope = w.enter_indent_scope();
                            let temp = format!("tmp{}", self.get_seq());
                            let elem_type_str = self.get_type(&ptr!(t.element_type))?;
                            w.writeln(&format!("let mut {} :{} = Default::default();", temp,elem_type_str))?;
                            self.write_decode_type(w, field, ptr!(t.element_type), &temp,&format!("{err_ident}[i]"))?;
                            w.writeln(&format!("{}.push({});", ident, temp))?;
                        }
                        w.writeln("}")?;
                    }
                }
                return Ok(());
            }
            _ => {}
        }
        Err(anyhow!("unsupported decode type"))
    }

    pub fn write_encode_field<W1: std::io::Write>(
        &mut self,
        w: &mut Writer<W1>,
        field: &SharedPtr<ast::Field>,
    ) -> Result<()> {
        if let ast::Type::UnionType(_) = ptr!(field.field_type) {
            return Ok(());
        }
        if let Some(_) = self.bit_field_parts.get(&PtrKey::new(field)) {
            return Ok(());
        }
        if let Some(f) = self.bit_fields.get(&PtrKey::new(field)) {
            self.write_encode_type(w, &field.borrow(), f.int_ty.clone(), &f.name.clone(),&f.name.clone())?;
            return Ok(());
        }
        let ident = self.expr_to_string(&ptr!(field.ident).into())?;
        let err_ident = ident.escape_default().to_string();
        self.write_encode_type(w, &field.borrow(), ptr!(field.field_type), &ident,&err_ident)?;
        Ok(())
    }

    pub fn write_decode_field<W1: std::io::Write>(
        &mut self,
        w: &mut Writer<W1>,
        field: &SharedPtr<ast::Field>,
    ) -> Result<()> {
        if let ast::Type::UnionType(_) = ptr!(field.field_type) {
            return Ok(());
        }
        if let Some(_) = self.bit_field_parts.get(&PtrKey::new(field)) {
            return Ok(());
        }
        if let Some(f) = self.bit_fields.get(&PtrKey::new(field)) {
            self.write_decode_type(w, &field.borrow(), f.int_ty.clone(), &f.name.clone(),&f.name.clone())?;
            return Ok(());
        }
        let ident = self.expr_to_string(&ptr!(field.ident).into())?;
        let err_ident = ident.escape_default().to_string();
        self.write_decode_type(w, &field.borrow(), ptr!(field.field_type), &ident,&err_ident)?;
        Ok(())
    }

    pub fn write_format_type<W1: std::io::Write>(
        &mut self,
        w: &mut Writer<W1>,
        fmt: SharedPtr<ast::Format>,
    ) -> Result<()> {
        let ident = ptr_null!(fmt.ident.ident);
        self.map_file.structs.push(ident.clone());
        w.writeln("#[derive(Default,Debug,Clone,PartialEq)]")?;
        w.writeln(&format!("pub struct {} {{", ident))?;
        {
            let _scope = w.enter_indent_scope();
            self.write_struct_type(w, ptr!(fmt.body.struct_type))?;
        }
        w.writeln("}")?;
        Ok(())
    }

    pub fn write_if<W1: std::io::Write>(
        &mut self,
        w: &mut Writer<W1>,
        node: SharedPtr<ast::If>,
    ) -> Result<()> {
        let cond = ptr!(node.cond);
        let cond = self
            .s
            .to_string(&cond.into())
            .map_err(|x| anyhow!("{:?}", x))?;
        w.writeln(&format!("if {cond} {{"))?;
        {
            let _scope = w.enter_indent_scope();
            self.write_node(w, ptr!(node.then).into())?;
        }
        w.writeln("}")?;
        if let Some(else_) = ptr_null!(node.els) {
            w.writeln("else ")?;
            if let ast::Node::If(els) = else_.clone().into() {
                self.write_if(w, els)?;
            } else {
                w.writeln("{")?;
                let _scope = w.enter_indent_scope();
                self.write_node(w, else_)?;
                w.writeln("}")?;
            }
        }
        Ok(())
    }

    pub fn write_match<W1: std::io::Write>(
        &mut self,
        w: &mut Writer<W1>,
        m: &SharedPtr<ast::Match>,
    ) -> Result<()> {
        if let Some(cond) = ptr_null!(m.cond) {
            w.writeln(&format!("match {} {{", self.expr_to_string(&cond.into())?))?;
        } else {
            w.writeln("match {")?;
        }
        for b in ptr_null!(m.branch) {
            let cond = &ptr!(b.cond).into();
            let cond = if is_any_range(cond) {
                "_".to_string()
            } else {
                self.expr_to_string(cond)?
            };
            w.writeln(&format!("{cond} => {{"))?;
            {
                let _scope = w.enter_indent_scope();
                self.write_node(w, ptr!(b.then).into())?;
            }
            w.writeln("},")?;
        }
        if !ptr_null!(m.struct_union_type.exhaustive) {
            w.writeln("_ => {}")?;
        }
        w.writeln("}")?;
        Ok(())
    }

    pub fn add_union_init<W1: std::io::Write>(
        &self,
        w: &mut Writer<W1>,
        t: &SharedPtr<ast::StructType>,
    ) -> Result<()> {
        if !self.encode {
            if let Some(anonymous) = self.structs.get(&PtrKey::new(t)) {
                let field = &anonymous.field;
                let ident = self.expr_to_string(&ptr!(field.ident).into())?;
                let name = &anonymous.name;
                let base = &anonymous.base;
                w.writeln(&format!("{ident} = {base}::{name}(Default::default());"))?;
            }
        }
        Ok(())
    }

    pub fn write_node<W1: std::io::Write>(
        &mut self,
        w: &mut Writer<W1>,
        node: ast::Node,
    ) -> Result<()> {
        match node {
            ast::Node::IndentBlock(block) => {
                self.add_union_init(w, &ptr!(block.struct_type))?;
                for elem in ptr_null!(block.elements).iter() {
                    self.write_node(w, elem.clone())?;
                }
            }
            ast::Node::ScopedStatement(stmt) => {
                self.add_union_init(w, &ptr!(stmt.struct_type))?;
                self.write_node(w, ptr!(stmt.statement).into())?;
            }
            ast::Node::If(if_) => {
                self.write_if(w, if_)?;
            }
            ast::Node::Match(m) => {
                self.write_match(w, &m)?;
            }
            ast::Node::Field(field) => {
                if self.encode {
                    self.write_encode_field(w, &field)?;
                } else {
                    self.write_decode_field(w, &field)?;
                }
            }
            ast::Node::ExplicitError(err) => {
                let msg = ptr_null!(err.message.value);
                w.writeln(&format!("return Err(Error::ExplicitError({}));",msg))?;
            }
            ast::Node::Binary(x) =>{
                if let ast::BinaryOp::DefineAssign | ast::BinaryOp::ConstAssign = ptr_null!(x.op) {
                    let ident :SharedPtr<ast::Ident> = ptr!(x.left).try_into().map_err(|x| anyhow!("{:?}",x))?;
                    let expr = self.expr_to_string(&ptr!(x.right).into())?;
                    let let_or_mut = if ptr_null!(x.op) == ast::BinaryOp::DefineAssign {
                        "let"
                    } else {
                        "let mut"
                    };
                    let escaped = Self::escape_keyword(ptr_null!(ident.ident));
                    w.writeln(&format!("{} {} = {};",let_or_mut,escaped,expr))?;
                    self.s.add_map(&ident, &escaped);
                }
            }
            _ => {} // TODO: Implement,skip for now
        }
        Ok(())
    }

    pub fn write_encode_fn<W1: std::io::Write>(
        &mut self,
        w: &mut Writer<W1>,
        ty: SharedPtr<ast::Format>,
    ) -> Result<()> {
        w.writeln(
            "pub fn encode<W:std::io::Write>(&self,w :&mut W) -> Result<(),Error> {",
        )?;
        {
            let _scope = w.enter_indent_scope();
            self.write_node(w, ast::Node::IndentBlock(ptr!(ty.body)))?;
            w.writeln("Ok(())")?;
        }
        w.writeln("}")?;
        w.writeln("pub fn encode_to_vec(&self) -> Result<Vec<u8>,Error> {")?;
        {
            let _scope = w.enter_indent_scope();
            w.writeln("let mut v = Vec::new();")?;
            w.writeln("self.encode(&mut v)?;")?;
            w.writeln("Ok(v)")?;
        }
        w.writeln("}")?;
        Ok(())
    }

    pub fn write_decode_fn<W1: std::io::Write>(
        &mut self,
        w: &mut Writer<W1>,
        ty: SharedPtr<ast::Format>,
    ) -> Result<()> {
        let ident = ptr_null!(ty.ident.ident);
        w.writeln(&format!(
            "pub fn decode<R:std::io::Read>(r :&mut R) -> Result<{ident},Error> {{"
        ))?;
        {
            let _scope = w.enter_indent_scope();
            w.writeln("let mut d = Self::default();")?;
            w.writeln("d.decode_impl(r)?;")?;
            w.writeln("Ok(d)")?;
        }
        w.writeln("}")?;
        w.writeln(&format!("pub fn decode_exact(r :&[u8]) -> Result<{ident},Error> {{",ident = ident))?;
        {
            let _scope = w.enter_indent_scope();
            w.writeln("let mut r = std::io::Cursor::new(r);")?;
            w.writeln("let result = Self::decode(&mut r)?;")?;
            w.writeln("if r.position() != r.get_ref().len() as u64 {")?;
            {
                let _scope = w.enter_indent_scope();
                w.writeln(&format!("return Err(Error::LengthError(\"not all bytes consumed\"))"))?;
            }
            w.writeln("}")?;
            w.writeln("Ok(result)")?;
        }
        w.writeln("}")?;
        w.writeln(&format!(
            "pub fn decode_impl<R :std::io::Read>(&mut self,r :&mut R) -> Result<(),Error> {{"
        ))?;
        {
            let _scope = w.enter_indent_scope();
            self.write_node(w, ast::Node::IndentBlock(ptr!(ty.body)))?;
            w.writeln("Ok(())")?;
        }
        w.writeln("}")?;
        Ok(())
    }

    pub fn write_format_fns<W1: std::io::Write>(
        &mut self,
        w: &mut Writer<W1>,
        fmt: SharedPtr<ast::Format>,
    ) -> Result<()> {
        //SAFETY: We are casting a mutable reference to a mutable reference, which is safe.
        w.writeln(&format!("impl {} {{", ptr_null!(fmt.ident.ident)))?;
        {
            let _scope = w.enter_indent_scope();
            self.s.error_string =
            "return Err(Error::UnwrapError(format!(\"unwrapping {:?} failed\",x))),".to_string();
            self.s.mutator = "&".to_string();
            self.encode = true;
            self.write_encode_fn(w, fmt.clone())?;
            self.s.mutator = "&mut ".to_string();
            self.encode = false;
            self.write_decode_fn(w, fmt.clone())?;
        }
        w.writeln("}")?;
        Ok(())
    }

    pub fn write_enum(&mut self, enum_: SharedPtr<ast::Enum>) -> Result<()> {
        let ident = ptr_null!(enum_.ident.ident);
        self.w.writeln("#[derive(Default,Debug,Clone,PartialEq)]")?;
        self.w.writeln(&format!("pub enum {} {{", ident))?;
        let mut first = true;
        {
            let _scope = self.w.enter_indent_scope();
            for elem in ptr_null!(enum_.members).iter() {
                if first {
                    self.w.writeln("#[default]")?;
                }
                self.w
                    .writeln(&format!("{},", ptr_null!(elem.ident.ident)))?;

                self.s.add_map(
                    &ptr!(elem.ident),
                    &format!("{}::{}", ident, ptr_null!(elem.ident.ident)),
                );
                first = false;
            }
            if let Some(base) = ptr_null!(enum_.base_type) {
                let base = self.get_type(&base)?;
                if first {
                    self.w.writeln("#[default]")?;
                }
                self.w.writeln(&format!("{}Other({}),", ident, base))?;
            }
        }
        self.w.writeln("}")?;
        if let Some(ast::Type::IntType(x)) = ptr_null!(enum_.base_type) {
            let bit_size = ptr!(x.bit_size);
            let base = get_int_type(
                bit_size,
                ptr_null!(x.is_signed),
                ptr_null!(x.is_common_supported),
            ).map_err(|x| anyhow!("{:?}", x))?;
            if bit_size >= 8 {
                self.w.writeln(&format!("impl {} {{", ident))?;
                {
                    let _scope = self.w.enter_indent_scope();
                    let mut write_bytes = |le_or_be: &str| -> Result<()> {
                        self.w.writeln(&format!(
                            "pub fn to_{le_or_be}_bytes(&self) -> [u8;{}] {{",
                            bit_size / 8
                        ))?;
                        {
                            let _scope = self.w.enter_indent_scope();
                            let tmp = format!("tmp{}", self.get_seq());
                            self.w
                                .writeln(&format!("let {tmp} :{base} = match self {{"))?;
                            {
                                let _scope = self.w.enter_indent_scope();
                                for elem in ptr_null!(enum_.members).iter() {
                                    self.w.writeln(&format!(
                                        "{}::{} => {},",
                                        ident,
                                        ptr_null!(elem.ident.ident),
                                        self.expr_to_string(&ptr!(elem.value))?
                                    ))?;
                                }
                            }
                            self.w
                                .writeln(&format!("{}::{}Other(x) => *x,", ident, ident))?;
                            self.w.writeln("};")?;
                            self.w.writeln(&format!("{tmp}.to_{le_or_be}_bytes()"))?;
                        }
                        self.w.writeln("}")?;

                        Ok(())
                    };
                    write_bytes("le")?;
                    write_bytes("be")?;
                }
                self.w.writeln("}")?;
            }

            self.w
                .writeln(&format!("impl From<{base}> for {ident} {{"))?;
            {
                let _scope = self.w.enter_indent_scope();
                self.w.writeln(&format!("fn from(x :{base}) -> Self {{"))?;
                {
                    let _scope = self.w.enter_indent_scope();
                    self.w.writeln("match x {")?;
                    {
                        let _scope = self.w.enter_indent_scope();
                        for elem in ptr_null!(enum_.members).iter() {
                            self.w.writeln(&format!(
                                "{} => {}::{},",
                                self.expr_to_string(&ptr!(elem.value))?,
                                ident,
                                ptr_null!(elem.ident.ident)
                            ))?;
                        }
                    }
                    self.w
                        .writeln(&format!("_ => {}::{}Other(x),", ident, ident))?;
                    self.w.writeln("}")?;
                }
                self.w.writeln("}")?;
            }
            self.w.writeln("}")?;

            self.w.writeln(&format!("impl From<{}> for {} {{", ident, base))?;
            {
                let _scope = self.w.enter_indent_scope();
                self.w.writeln(&format!("fn from(x :{ident}) -> {base} {{"))?;
                {
                    let _scope = self.w.enter_indent_scope();
                    self.w.writeln("match x {")?;
                    {
                        let _scope = self.w.enter_indent_scope();
                        for elem in ptr_null!(enum_.members).iter() {
                            self.w.writeln(&format!(
                                "{}::{} => {},",
                                ident,
                                ptr_null!(elem.ident.ident),
                                self.expr_to_string(&ptr!(elem.value))?
                            ))?;
                        }
                    }
                    self.w.writeln(&format!(
                        "{}::{}Other(x) => x,",
                        ident, ident
                    ))?;
                    self.w.writeln("}")?;
                }
                self.w.writeln("}")?;
            }
            self.w.writeln("}")?;
        }
        Ok(())
    }

    pub fn write_error_type(&mut self) -> Result<()> {

        self.w.writeln("#[derive(Debug)]")?;
        self.w.writeln("pub enum Error {")?;
        {
            let _scope = self.w.enter_indent_scope();
            self.w.writeln("Io(&'static str,std::io::Error),")?;
            self.w.writeln("UnwrapError(String),")?;
            self.w.writeln("LengthError(&'static str),")?;
            self.w.writeln("SetError(&'static str),")?;    
            self.w.writeln("Nested(&'static str,Box<Error>),")?;
            self.w.writeln("ExplicitError(&'static str),")?;
        }
        self.w.writeln("}")?;

        self.w.writeln("impl Error {")?;
        self.w.writeln("pub fn io_error(&self) -> Option<&std::io::Error> {")?;
        {
            let _scope = self.w.enter_indent_scope();
            self.w.writeln("match self {")?;
            {
                let _scope = self.w.enter_indent_scope();
                self.w.writeln("Error::Io(_,e) => Some(e),")?;
                self.w.writeln("Error::Nested(_,e) => e.io_error(),")?;
                self.w.writeln("_ => None,")?;
            }
            self.w.writeln("}")?;
        }
        self.w.writeln("}")?;
        self.w.writeln("}")?;

        self.w.writeln("impl std::fmt::Display for Error {")?;
        {
            let _scope = self.w.enter_indent_scope();
            self.w.writeln("fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {")?;
            {
                let _scope = self.w.enter_indent_scope();
                self.w.writeln("match self {")?;
                {
                    let _scope = self.w.enter_indent_scope();
                    self.w.writeln("Error::Io(s,e) => write!(f,\"{}: {}\",s,e),")?;
                    self.w.writeln("Error::UnwrapError(s) => write!(f,\"{}\",s),")?;
                    self.w.writeln("Error::LengthError(s) => write!(f,\"{}\",s),")?;
                    self.w.writeln("Error::SetError(s) => write!(f,\"{}\",s),")?;
                    self.w.writeln("Error::Nested(s,e) => write!(f,\"{}: {}\",s,e),")?;
                    self.w.writeln("Error::ExplicitError(s) => write!(f,\"{}\",s),")?;
                }
                self.w.writeln("}")?;
            }
            self.w.writeln("}")?;
        }
        self.w.writeln("}")?;

        self.w.writeln("impl std::error::Error for Error {}")?;

        Ok(())
    }


    pub fn write_program(&mut self, in_prog: SharedPtr<ast::Program>) -> Result<()> {
        let prog = in_prog.borrow();
        self.w.writeln("// Code generated by json2rust. DO NOT EDIT.")?;
        self.write_error_type()?;
        for elem in prog.elements.iter() {
            if let Ok(enum_) = elem.try_into() {
                self.write_enum(enum_)?;
            }
        }
        let sorted = topological_sort_format(&in_prog).ok_or(anyhow!("sort error"))?;
        let mut w = Writer::new(Vec::new());
        w.prefix = "$SELF".to_string();
        for elem in sorted.iter() {
            self.write_format_type(&mut w, elem.clone())?;
        }
        for elem in sorted {
            self.write_format_fns(&mut w, elem)?;
        }
        self.w.write(&String::from_utf8(w.writer)?)?;
        self.w.flush()?;
        Ok(())
    }
}
