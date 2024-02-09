package main

import (
	"io"
	"log"
	"os"
	"path/filepath"

	"github.com/iancoleman/strcase"
	"github.com/on-keyday/brgen/src/tool/gen"
)

func generate(rw io.Writer, defs *gen.Defs) {
	w := gen.NewWriter(rw)
	w.Printf("// Code generated by ast2rust. DO NOT EDIT.\n")
	w.Printf("use std::rc::{Rc,Weak};\n")
	w.Printf("use std::cell::RefCell;\n")
	w.Printf("use std::convert::TryFrom;\n")
	w.Printf("use serde_derive::{Serialize,Deserialize};\n\n")
	w.Printf("use std::collections::HashMap;\n\n")

	w.Printf("#[derive(Debug)]\n")
	w.Printf("pub enum JSONType {\n")
	w.Printf("	Null,\n")
	w.Printf("	Bool,\n")
	w.Printf("	Number,\n")
	w.Printf("	String,\n")
	w.Printf("	Array,\n")
	w.Printf("	Object,\n")
	w.Printf("}\n\n")

	w.Printf("impl From<&serde_json::Value> for JSONType {\n")
	w.Printf("	fn from(v:&serde_json::Value)->Self{\n")
	w.Printf("		match v{\n")
	w.Printf("			serde_json::Value::Null=>Self::Null,\n")
	w.Printf("			serde_json::Value::Bool(_)=>Self::Bool,\n")
	w.Printf("			serde_json::Value::Number(_)=>Self::Number,\n")
	w.Printf("			serde_json::Value::String(_)=>Self::String,\n")
	w.Printf("			serde_json::Value::Array(_)=>Self::Array,\n")
	w.Printf("			serde_json::Value::Object(_)=>Self::Object,\n")
	w.Printf("		}\n")
	w.Printf("	}\n")
	w.Printf("}\n\n")

	w.Printf("#[derive(Debug)]\n")
	w.Printf("pub enum Error {\n")
	w.Printf("	UnknownNodeType(NodeType),\n")
	w.Printf("	MismatchNodeType(NodeType,NodeType),\n")
	w.Printf("	JSONError(serde_json::Error),\n")
	w.Printf("	MissingField(NodeType,&'static str),\n")
	w.Printf("	MismatchJSONType(JSONType,JSONType),\n")
	w.Printf("	InvalidNodeType(NodeType),\n")
	w.Printf(" 	InvalidRawNodeType(String),\n")
	w.Printf("	IndexOutOfBounds(usize),\n")
	w.Printf("	InvalidEnumValue(String),\n")
	w.Printf("}\n\n")

	w.Printf("impl From<&Node> for NodeType {\n")
	w.Printf("	fn from(node:&Node)-> Self{\n")
	w.Printf("		match node {\n")
	for _, nodeType := range defs.NodeTypes {
		typ := strcase.ToCamel(nodeType)
		_, ok := defs.Structs[typ]
		if !ok {
			continue
		}
		w.Printf("			Node::%s(_) => Self::%s,\n", typ, typ)
	}
	w.Printf("		}\n")
	w.Printf("	}\n")
	w.Printf("}\n\n")

	w.Printf("impl From<Node> for NodeType {\n")
	w.Printf("	fn from(node:Node)-> Self{\n")
	w.Printf("		Self::from(&node)\n")
	w.Printf("	}\n")
	w.Printf("}\n\n")

	w.Printf("impl From<&NodeWeak> for NodeType {\n")
	w.Printf("	fn from(node:&NodeWeak)-> Self{\n")
	w.Printf("		match node {\n")
	for _, nodeType := range defs.NodeTypes {
		typ := strcase.ToCamel(nodeType)
		_, ok := defs.Structs[typ]
		if !ok {
			continue
		}
		w.Printf("			NodeWeak::%s(_) => Self::%s,\n", typ, typ)
	}
	w.Printf("		}\n")
	w.Printf("	}\n")
	w.Printf("}\n\n")

	w.Printf("impl From<NodeWeak> for NodeType {\n")
	w.Printf("	fn from(node:NodeWeak)-> Self{\n")
	w.Printf("		Self::from(&node)\n")
	w.Printf("	}\n")
	w.Printf("}\n\n")

	for _, def := range defs.Defs {
		switch d := def.(type) {
		case *gen.Interface:
			w.Printf("#[derive(Debug,Clone)]\n")
			w.Printf("pub enum %s {\n", d.Name)
			for _, field := range d.Derived {
				found, ok := defs.Structs[field]
				if !ok {
					continue
				}
				w.Printf("	%s(Rc<RefCell<%s>>),\n", field, found.Name)
			}
			w.Printf("}\n\n")

			w.Printf("#[derive(Debug,Clone)]\n")
			w.Printf("pub enum %sWeak {\n", d.Name)
			for _, field := range d.Derived {
				found, ok := defs.Structs[field]
				if !ok {
					continue
				}
				w.Printf("	%s(Weak<RefCell<%s>>),\n", field, found.Name)
			}
			w.Printf("}\n\n")

			w.Printf("impl %s {\n", d.Name)
			for _, field := range d.Fields {
				w.Printf("    pub fn get_%s(&self)-> %s {\n", field.Name, field.Type.RustString())
				w.Printf("        match self {\n")
				for _, derived := range d.Derived {
					_, ok := defs.Structs[derived]
					if !ok {
						continue
					}
					w.Printf("            %s::%s(node)=>node.borrow().%s.clone(),\n", d.Name, derived, field.Name)
				}
				w.Printf("        }\n")
				w.Printf("    }\n")
			}
			w.Printf("}\n\n")

			w.Printf("impl From<&%s> for %sWeak {\n", d.Name, d.Name)
			w.Printf("	fn from(node:&%s)-> Self{\n", d.Name)
			w.Printf("		match node {\n")
			for _, field := range d.Derived {
				_, ok := defs.Structs[field]
				if !ok {
					continue
				}
				w.Printf("			%s::%s(node)=>Self::%s(Rc::downgrade(node)),\n", d.Name, field, field)
			}
			w.Printf("		}\n")
			w.Printf("	}\n")
			w.Printf("}\n\n")

			w.Printf("impl From<%s> for %sWeak {\n", d.Name, d.Name)
			w.Printf("	fn from(node:%s)-> Self{\n", d.Name)
			w.Printf("		Self::from(&node)\n")
			w.Printf("	}\n")
			w.Printf("}\n\n")

			w.Printf("impl TryFrom<&%sWeak> for %s {\n", d.Name, d.Name)
			w.Printf("	type Error = Error;\n")
			w.Printf("	fn try_from(node:&%sWeak)->Result<Self,Self::Error>{\n", d.Name)
			w.Printf("		match node {\n")
			for _, field := range d.Derived {
				found, ok := defs.Structs[field]
				if !ok {
					continue
				}
				w.Printf("			%sWeak::%s(node)=>Ok(Self::%s(node.upgrade().ok_or(Error::InvalidNodeType(NodeType::%s))?)),\n", d.Name, field, field, found.Name)
			}
			w.Printf("		}\n")
			w.Printf("	}\n")
			w.Printf("}\n\n")

			w.Printf("impl TryFrom<%sWeak> for %s {\n", d.Name, d.Name)
			w.Printf("	type Error = Error;\n")
			w.Printf("	fn try_from(node:%sWeak)->Result<Self,Self::Error>{\n", d.Name)
			w.Printf("		Self::try_from(&node)\n")
			w.Printf("	}\n")
			w.Printf("}\n\n")

			if d.Name == "Node" {
				continue
			}

			w.Printf("impl TryFrom<&Node> for %s {\n", d.Name)
			w.Printf("	type Error = Error;\n")
			w.Printf("	fn try_from(node:&Node)->Result<Self,Self::Error>{\n")
			w.Printf("		match node {\n")
			for _, field := range d.Derived {
				found, ok := defs.Structs[field]
				if !ok {
					continue
				}
				w.Printf("			Node::%s(node)=>Ok(Self::%s(node.clone())),\n", found.Name, field)
			}
			w.Printf("			_=> Err(Error::InvalidNodeType(node.into())),\n")
			w.Printf("		}\n")
			w.Printf("	}\n")
			w.Printf("}\n\n")

			w.Printf("impl TryFrom<Node> for %s {\n", d.Name)
			w.Printf("	type Error = Error;\n")
			w.Printf("	fn try_from(node:Node)->Result<Self,Self::Error>{\n")
			w.Printf("		Self::try_from(&node)\n")
			w.Printf("	}\n")
			w.Printf("}\n\n")

			w.Printf("impl From<&%s> for Node {\n", d.Name)
			w.Printf("	fn from(node:&%s)-> Self{\n", d.Name)
			w.Printf("		match node {\n")
			for _, field := range d.Derived {
				found, ok := defs.Structs[field]
				if !ok {
					continue
				}
				w.Printf("			%s::%s(node)=>Self::%s(node.clone()),\n", d.Name, field, found.Name)
			}
			w.Printf("		}\n")
			w.Printf("	}\n")
			w.Printf("}\n\n")

			w.Printf("impl From<%s> for Node {\n", d.Name)
			w.Printf("	fn from(node:%s)-> Self{\n", d.Name)
			w.Printf("		Self::from(&node)\n")
			w.Printf("	}\n")
			w.Printf("}\n\n")

			w.Printf("impl TryFrom<&%sWeak> for Node {\n", d.Name)
			w.Printf("	type Error = Error;\n")
			w.Printf("	fn try_from(node:&%sWeak)->Result<Self,Self::Error>{\n", d.Name)
			w.Printf("		match node {\n")
			for _, field := range d.Derived {
				found, ok := defs.Structs[field]
				if !ok {
					continue
				}
				w.Printf("			%sWeak::%s(node)=>Ok(Self::%s(node.upgrade().ok_or(Error::InvalidNodeType(NodeType::%s))?)),\n", d.Name, field, found.Name, found.Name)
			}
			w.Printf("		}\n")
			w.Printf("	}\n")
			w.Printf("}\n\n")

			w.Printf("impl TryFrom<%sWeak> for Node {\n", d.Name)
			w.Printf("	type Error = Error;\n")
			w.Printf("	fn try_from(node:%sWeak)->Result<Self,Self::Error>{\n", d.Name)
			w.Printf("		Self::try_from(&node)\n")
			w.Printf("	}\n")
			w.Printf("}\n\n")

			w.Printf("impl From<&%sWeak> for NodeWeak {\n", d.Name)
			w.Printf("	fn from(node:&%sWeak)-> Self{\n", d.Name)
			w.Printf("		match node {\n")
			for _, field := range d.Derived {
				_, ok := defs.Structs[field]
				if !ok {
					continue
				}
				w.Printf("			%sWeak::%s(node)=>Self::%s(node.clone()),\n", d.Name, field, field)
			}
			w.Printf("		}\n")
			w.Printf("	}\n")
			w.Printf("}\n\n")

			w.Printf("impl From<%sWeak> for NodeWeak {\n", d.Name)
			w.Printf("	fn from(node:%sWeak)-> Self{\n", d.Name)
			w.Printf("		Self::from(&node)\n")
			w.Printf("	}\n")
			w.Printf("}\n\n")

			w.Printf("impl TryFrom<&NodeWeak> for %sWeak {\n", d.Name)
			w.Printf("	type Error = Error;\n")
			w.Printf("	fn try_from(node:&NodeWeak)->Result<Self,Self::Error>{\n")
			w.Printf("		match node {\n")
			for _, field := range d.Derived {
				_, ok := defs.Structs[field]
				if !ok {
					continue
				}
				w.Printf("			NodeWeak::%s(node)=>Ok(Self::%s(node.clone())),\n", field, field)
			}
			w.Printf("			_=> Err(Error::InvalidNodeType(node.into())),\n")
			w.Printf("		}\n")
			w.Printf("	}\n")
			w.Printf("}\n\n")

			w.Printf("impl TryFrom<NodeWeak> for %sWeak {\n", d.Name)
			w.Printf("	type Error = Error;\n")
			w.Printf("	fn try_from(node:NodeWeak)->Result<Self,Self::Error>{\n")
			w.Printf("		Self::try_from(&node)\n")
			w.Printf("	}\n")
			w.Printf("}\n\n")

			w.Printf("impl TryFrom<&Node> for %sWeak {\n", d.Name)
			w.Printf("	type Error = Error;\n")
			w.Printf("	fn try_from(node:&Node)->Result<Self,Self::Error>{\n")
			w.Printf("		match node {\n")
			for _, field := range d.Derived {
				_, ok := defs.Structs[field]
				if !ok {
					continue
				}
				w.Printf("			Node::%s(node)=>Ok(Self::%s(Rc::downgrade(node))),\n", field, field)
			}
			w.Printf("			_=> Err(Error::InvalidNodeType(node.into())),\n")
			w.Printf("		}\n")
			w.Printf("	}\n")
			w.Printf("}\n\n")

			w.Printf("impl TryFrom<Node> for %sWeak {\n", d.Name)
			w.Printf("	type Error = Error;\n")
			w.Printf("	fn try_from(node:Node)->Result<Self,Self::Error>{\n")
			w.Printf("		Self::try_from(&node)\n")
			w.Printf("	}\n")
			w.Printf("}\n\n")

		case *gen.Struct:
			if d.Name == "Loc" || d.Name == "Pos" {
				w.Printf("#[derive(Debug,Clone,Copy,Serialize,Deserialize)]\n")
			} else if d.NodeType == "" && d.Name != "Scope" {
				w.Printf("#[derive(Debug,Clone,Serialize,Deserialize)]\n")
			} else {
				w.Printf("#[derive(Debug,Clone)]\n")
			}
			w.Printf("pub struct %s {\n", d.Name)
			for _, field := range d.Fields {
				w.Printf("	pub %s: %s,\n", field.Name, field.Type.RustString())
			}
			w.Printf("}\n\n")

			for _, impl := range d.Implements {
				w.Printf("impl TryFrom<&%s> for Rc<RefCell<%s>> {\n", impl, d.Name)
				w.Printf("	type Error = Error;\n")
				w.Printf("	fn try_from(node:&%s)->Result<Self,Self::Error>{\n", impl)
				w.Printf("		match node {\n")
				w.Printf("			%s::%s(node)=>Ok(node.clone()),\n", impl, d.Name)
				if impl == "Node" {
					w.Printf("			_=> Err(Error::InvalidNodeType(node.into())),\n")
				} else {
					w.Printf("			_=> Err(Error::InvalidNodeType(Node::from(node).into())),\n")
				}
				w.Printf("		}\n")
				w.Printf("	}\n")
				w.Printf("}\n\n")

				w.Printf("impl TryFrom<%s> for Rc<RefCell<%s>> {\n", impl, d.Name)
				w.Printf("	type Error = Error;\n")
				w.Printf("	fn try_from(node:%s)->Result<Self,Self::Error>{\n", impl)
				w.Printf("		Self::try_from(&node)\n")
				w.Printf("	}\n")
				w.Printf("}\n\n")

				w.Printf("impl From<&Rc<RefCell<%s>>> for %s {\n", d.Name, impl)
				w.Printf("	fn from(node:&Rc<RefCell<%s>>)-> Self{\n", d.Name)
				w.Printf("		%s::%s(node.clone())\n", impl, d.Name)
				w.Printf("	}\n")
				w.Printf("}\n\n")

				w.Printf("impl From<Rc<RefCell<%s>>> for %s {\n", d.Name, impl)
				w.Printf("	fn from(node:Rc<RefCell<%s>>)-> Self{\n", d.Name)
				w.Printf("		Self::from(&node)\n")
				w.Printf("	}\n")
				w.Printf("}\n\n")
			}

		case *gen.Enum:
			w.Printf("#[derive(Debug,Clone,Copy,Serialize,Deserialize)]\n")
			w.Printf("#[serde(rename_all = \"snake_case\")]")
			w.Printf("pub enum %s {\n", d.Name)
			for _, field := range d.Values {
				field.Name = strcase.ToCamel(field.Name)
				w.Printf("	%s,\n", field.Name)
			}
			w.Printf("}\n\n")
			w.Printf("impl TryFrom<&str> for %s {\n", d.Name)
			w.Printf("	type Error = ();\n")
			w.Printf("	fn try_from(s:&str)->Result<Self,()>{\n")
			w.Printf("		match s{\n")
			for _, field := range d.Values {
				w.Printf("			%q =>Ok(Self::%s),\n", field.Value, field.Name)
			}
			w.Printf("			_=> Err(()),\n")
			w.Printf("		}\n")
			w.Printf("	}\n")
			w.Printf("}\n\n")
		}
	}

	w.Printf("pub fn parse_ast(ast:JsonAst)->Result<Rc<RefCell<Program>> ,Error>{\n")

	w.Printf("	let mut nodes = Vec::new();\n")
	w.Printf("	let mut scopes = Vec::new();\n")
	w.Printf("	for raw_node in &ast.node{\n")
	w.Printf("		let node_type :NodeType = match raw_node.node_type.as_str().try_into() {\n")
	w.Printf("			Ok(v)=>v,\n")
	w.Printf("			Err(_) =>return Err(Error::InvalidRawNodeType(raw_node.node_type.clone())),\n")
	w.Printf("		};\n")
	w.Printf("		let node = match node_type {\n")
	for _, nodeType := range defs.Defs {
		switch d := nodeType.(type) {
		case *gen.Struct:
			if len(d.Implements) == 0 {
				continue
			}
			w.Printf("			NodeType::%s => {\n", d.Name)
			w.Printf("				Node::%s(Rc::new(RefCell::new(%s {\n", d.Name, d.Name)
			for _, field := range d.Fields {
				if field.Type.IsArray {
					w.Printf("				%s: Vec::new(),\n", field.Name)
				} else if field.Type.IsPtr || field.Type.IsInterface {
					w.Printf("				%s: None,\n", field.Name)
				} else if field.Type.Name == "u64" {
					if field.Type.IsOptional {
						w.Printf("				%s: None,\n", field.Name)
					} else {
						w.Printf("				%s: 0,\n", field.Name)
					}
				} else if field.Type.Name == "String" {
					w.Printf("				%s: String::new(),\n", field.Name)
				} else if field.Type.Name == "bool" {
					w.Printf("				%s: false,\n", field.Name)
				} else if field.Type.Name == "Loc" {
					w.Printf("				%s: raw_node.loc.clone(),\n", field.Name)
				} else {
					found, ok := defs.Enums[field.Type.Name]
					if !ok {
						continue
					}
					w.Printf("				%s: %s::%s,\n", field.Name, field.Type.Name, found.Values[0].Name)
				}
			}
			w.Printf("				})))\n")
			w.Printf("			},\n")
		}
	}
	w.Printf("			_=>return Err(Error::UnknownNodeType(node_type)),\n")
	w.Printf("		};\n")
	w.Printf("		nodes.push(node);\n")
	w.Printf("	}\n")
	w.Printf("	for _ in &ast.scope{\n")
	w.Printf("		let scope = Rc::new(RefCell::new(Scope{\n")
	for _, field := range defs.ScopeDef.Fields {
		if field.Type.IsArray {
			w.Printf("			%s: Vec::new(),\n", field.Name)
		} else if field.Type.IsPtr || field.Type.IsInterface {
			w.Printf("			%s: None,\n", field.Name)
		} else if field.Type.Name == "bool" {
			w.Printf("			%s: false,\n", field.Name)
		} else if field.Type.Name == "u64" {
			w.Printf("			%s: 0,\n", field.Name)
		} else if field.Type.Name == "String" {
			w.Printf("			%s: String::new(),\n", field.Name)
		}
	}
	w.Printf("		}));\n")
	w.Printf("		scopes.push(scope);\n")
	w.Printf("	}\n")

	w.Printf("	for (i,raw_node) in ast.node.into_iter().enumerate(){\n")
	w.Printf("		let node_type :NodeType = match raw_node.node_type.as_str().try_into() {\n")
	w.Printf("			Ok(v)=>v,\n")
	w.Printf("			Err(_) =>return Err(Error::InvalidRawNodeType(raw_node.node_type.clone())),\n")
	w.Printf("		};\n")
	w.Printf("		match node_type {\n")
	for _, nodeType := range defs.Defs {
		switch d := nodeType.(type) {
		case *gen.Struct:
			if len(d.Implements) == 0 {
				continue
			}
			w.Printf("			NodeType::%s => {\n", d.Name)
			w.Printf("				let node = nodes[i].clone();\n")
			if len(d.Fields) > 1 {
				w.Printf("				let node = match node {\n")
			} else {
				w.Printf("				let _ = match node {\n")
			}
			w.Printf("					Node::%s(node)=>node,\n", d.Name)
			w.Printf("					_=>return Err(Error::MismatchNodeType(node_type,node.into())),\n")
			w.Printf("				};\n")
			indexNodeFn := func(indent, val, bulk, name string) {
				w.Printf(indent+"let %s = match %s.get(%s as usize) {\n", val, bulk, name)
				w.Printf(indent + "	Some(v)=>v,\n")
				w.Printf(indent+"	None => return Err(Error::IndexOutOfBounds(%s as usize)),\n", name)
				w.Printf(indent + "};\n")
			}
			for _, field := range d.Fields {
				if field.Name == "loc" {
					continue
				}
				w.Printf("				let %s_body = match raw_node.body.get(%q) {\n", field.Name, field.Tag)
				w.Printf("					Some(v)=>v,\n")
				w.Printf("					None=>return Err(Error::MissingField(node_type,%q)),\n", field.Tag)
				w.Printf("				};\n")
				if field.Type.IsArray {
					w.Printf("				let %s_body = match %s_body.as_array(){\n", field.Name, field.Name)
					w.Printf("					Some(v)=>v,\n")
					w.Printf("					None=>return Err(Error::MismatchJSONType(%s_body.into(),JSONType::Array)),\n", field.Name)
					w.Printf("				};\n")
					w.Printf("				for link in %s_body {\n", field.Name)
					w.Printf("					let link = match link.as_u64() {\n")
					w.Printf("						Some(v)=>v,\n")
					w.Printf("						None=>return Err(Error::MismatchJSONType(link.into(),JSONType::Number)),\n")
					w.Printf("					};\n")
					indexNodeFn("					", field.Name+"_body", "nodes", "link")
					if field.Type.IsInterface {
						if field.Type.Name == "Node" {
							w.Printf("					node.borrow_mut().%s.push(%s_body.clone());\n", field.Name, field.Name)
						} else {
							w.Printf("					node.borrow_mut().%s.push(%s_body.try_into()?);\n", field.Name, field.Name)
						}
					} else {
						w.Printf("					let %s_body = match %s_body {\n", field.Name, field.Name)
						w.Printf("						Node::%s(body)=>body,\n", field.Type.Name)
						w.Printf("						x =>return Err(Error::MismatchNodeType(x.into(),%s_body.into())),\n", field.Name)
						w.Printf("					};\n")
						if field.Type.IsWeak {
							w.Printf("					node.borrow_mut().%s.push(Rc::downgrade(&%s_body));\n", field.Name, field.Name)
						} else {
							w.Printf("					node.borrow_mut().%s.push(%s_body.clone());\n", field.Name, field.Name)
						}
					}
					w.Printf("				}\n")
				} else if field.Type.IsPtr || field.Type.IsInterface {
					w.Printf(" 				if !%s_body.is_null() {\n", field.Name)
					w.Printf("					let %s_body = match %s_body.as_u64() {\n", field.Name, field.Name)
					w.Printf("						Some(v)=>v,\n")
					w.Printf("						None=>return Err(Error::MismatchJSONType(%s_body.into(),JSONType::Number)),\n", field.Name)
					w.Printf("					};\n")
					if field.Type.Name == "Scope" {
						indexNodeFn("					", field.Name+"_body", "scopes", field.Name+"_body")
						w.Printf("					node.borrow_mut().%s = Some(%s_body.clone());\n", field.Name, field.Name)
					} else {
						indexNodeFn("					", field.Name+"_body", "nodes", field.Name+"_body")
						if field.Type.IsInterface {
							if field.Type.Name == "Node" {
								if field.Type.IsWeak {
									w.Printf("					node.borrow_mut().%s = Some(%sWeak::from(%s_body.clone()));\n", field.Name, field.Type.Name, field.Name)
								} else {
									w.Printf("					node.borrow_mut().%s = Some(%s_body.clone());\n", field.Name, field.Name)
								}
							} else {
								w.Printf("					node.borrow_mut().%s = Some(%s_body.try_into()?);\n", field.Name, field.Name)
							}
						} else {
							w.Printf("					let %s_body = match %s_body {\n", field.Name, field.Name)
							w.Printf("						Node::%s(node)=>node,\n", field.Type.Name)
							w.Printf("						x =>return Err(Error::MismatchNodeType(x.into(),%s_body.into())),\n", field.Name)
							w.Printf("					};\n")
							if field.Type.IsWeak {
								w.Printf("					node.borrow_mut().%s = Some(Rc::downgrade(&%s_body));\n", field.Name, field.Name)
							} else {
								w.Printf("					node.borrow_mut().%s = Some(%s_body.clone());\n", field.Name, field.Name)
							}
						}
					}
					w.Printf("				}\n")
				} else if field.Type.Name == "u64" {
					w.Printf("				node.borrow_mut().%s = match %s_body.as_u64() {\n", field.Name, field.Name)
					if field.Type.IsOptional {
						w.Printf("					Some(v)=>Some(v),\n")
						w.Printf("					None=> match %s_body.is_null() {\n", field.Name)
						w.Printf("						true=>None,\n")
						w.Printf("						false=>return Err(Error::MismatchJSONType(%s_body.into(),JSONType::Number)),\n", field.Name)
						w.Printf("					},\n")
					} else {
						w.Printf("					Some(v)=>v,\n")
						w.Printf("					None=>return Err(Error::MismatchJSONType(%s_body.into(),JSONType::Number)),\n", field.Name)
					}
					w.Printf("				};\n")
				} else if field.Type.Name == "String" {
					w.Printf("				node.borrow_mut().%s = match %s_body.as_str() {\n", field.Name, field.Name)
					w.Printf("					Some(v)=>v.to_string(),\n")
					w.Printf("					None=>return Err(Error::MismatchJSONType(%s_body.into(),JSONType::String)),\n", field.Name)
					w.Printf("				};\n")
				} else if field.Type.Name == "bool" {
					w.Printf("				node.borrow_mut().%s = match %s_body.as_bool() {\n", field.Name, field.Name)
					w.Printf("					Some(v)=>v,\n")
					w.Printf("					None=>return Err(Error::MismatchJSONType(%s_body.into(),JSONType::Bool)),\n", field.Name)
					w.Printf("				};\n")
				} else if field.Type.Name == "Loc" {
					w.Printf("				node.borrow_mut().%s = match serde_json::from_value(%s_body.clone()) {\n", field.Name, field.Name)
					w.Printf("					Ok(v)=>v,\n")
					w.Printf("					Err(e)=>return Err(Error::JSONError(e)),\n")
					w.Printf("				};\n")
				} else {
					_, ok := defs.Enums[field.Type.Name]
					if !ok {
						continue
					}
					w.Printf("				node.borrow_mut().%s = match %s_body.as_str() {\n", field.Name, field.Name)
					w.Printf("					Some(v)=>match %s::try_from(v) {\n", field.Type.Name)
					w.Printf("						Ok(v)=>v,\n")
					w.Printf("						Err(_) => return Err(Error::InvalidEnumValue(v.to_string())),\n")
					w.Printf("					},\n")
					w.Printf("					None=>return Err(Error::MismatchJSONType(%s_body.into(),JSONType::String)),\n", field.Name)
					w.Printf("				};\n")
				}
			}
			w.Printf("			},\n")
		}
	}
	w.Printf("			_=>return Err(Error::UnknownNodeType(node_type)),\n")
	w.Printf("		};\n")
	w.Printf("	}\n")

	w.Printf("	for (i,raw_scope) in ast.scope.into_iter().enumerate(){\n")
	w.Printf("		let scope = scopes[i].clone();\n")
	w.Printf("		if let Some(owner) = raw_scope.owner {\n")
	w.Printf("			let owner = match nodes.get(owner as usize) {\n")
	w.Printf("				Some(v)=>v,\n")
	w.Printf("				None =>return Err(Error::IndexOutOfBounds(owner as usize)),\n")
	w.Printf("			};\n")
	w.Printf("			scope.borrow_mut().owner = Some(NodeWeak::from(owner));\n")
	w.Printf("		}\n")
	w.Printf("		if let Some(prev) = raw_scope.prev{\n")
	w.Printf("			let prev = match scopes.get(prev as usize) {\n")
	w.Printf("				Some(v)=>v,\n")
	w.Printf("				None =>return Err(Error::IndexOutOfBounds(prev as usize)),\n")
	w.Printf("			};\n")
	w.Printf("			scope.borrow_mut().prev = Some(Rc::downgrade(&prev));\n")
	w.Printf("		}\n")
	w.Printf("		if let Some(next) = raw_scope.next{\n")
	w.Printf("			let next = match scopes.get(next as usize) {\n")
	w.Printf("				Some(v)=>v,\n")
	w.Printf("				None =>return Err(Error::IndexOutOfBounds(next as usize)),\n")
	w.Printf("			};\n")
	w.Printf("			scope.borrow_mut().next = Some(next.clone());\n")
	w.Printf("		}\n")
	w.Printf("		if let Some(branch) = raw_scope.branch{\n")
	w.Printf("			let branch = match scopes.get(branch as usize) {\n")
	w.Printf("				Some(v)=>v,\n")
	w.Printf("				None =>return Err(Error::IndexOutOfBounds(branch as usize)),\n")
	w.Printf("			};\n")
	w.Printf("			scope.borrow_mut().branch = Some(branch.clone());\n")
	w.Printf("		}\n")
	w.Printf("		for ident in &raw_scope.ident{\n")
	w.Printf("			let ident = match nodes.get(*ident as usize) {\n")
	w.Printf("				Some(v)=>v,\n")
	w.Printf("				None =>return Err(Error::IndexOutOfBounds(*ident as usize)),\n")
	w.Printf("			};\n")
	w.Printf(" 			let ident = match ident {\n")
	w.Printf("				Node::Ident(v)=> v,\n")
	w.Printf("				_=>return Err(Error::MismatchNodeType(NodeType::Ident,ident.into())),\n")
	w.Printf("			};\n")
	w.Printf("			scope.borrow_mut().ident.push(Rc::downgrade(ident));\n")
	w.Printf("		}\n")
	w.Printf("	}\n\n")

	w.Printf("	match nodes.get(0){\n")
	w.Printf("		Some(v)=> match v {\n")
	w.Printf("			Node::Program(v)=>Ok(v.clone()),\n")
	w.Printf("			_=> Err(Error::MismatchNodeType(NodeType::Program,v.into())),\n")
	w.Printf("		},\n")
	w.Printf("		None=>Err(Error::IndexOutOfBounds(0)),\n")
	w.Printf("	}\n")
	w.Printf("}\n\n")

	w.Printf("pub trait Visitor {\n")
	w.Printf("	fn visit(&self,node:&Node)->bool;\n")
	w.Printf("}\n\n")

	w.Printf("impl<F :Fn(&dyn Visitor,&Node)->bool> Visitor for F {\n")
	w.Printf("	fn visit(&self,node:&Node)->bool{\n")
	w.Printf("		self(self,node)\n")
	w.Printf("	}\n")
	w.Printf("}\n\n")

	w.Printf("pub fn walk_node<'a,F>(node:&Node,f:&'a F)\n")
	w.Printf("where\n")
	w.Printf("	F: Visitor+?Sized+'a\n")
	w.Printf("{\n")
	w.Printf("	match node {\n")
	for _, nodeType := range defs.Defs {
		switch d := nodeType.(type) {
		case *gen.Struct:
			if len(d.Implements) == 0 {
				continue
			}
			once := true
			writeOnce := func() {
				if once {
					w.Printf("		Node::%s(node)=>{\n", d.Name)
					once = false
				}
			}
			for _, field := range d.Fields {
				if field.Type.Name == "Scope" {
					continue
				}
				if field.Type.IsWeak {
					continue
				}
				if field.Type.IsArray {
					writeOnce()
					w.Printf("			for node in &node.borrow().%s{\n", field.Name)
					if field.Type.Name == "Node" {
						w.Printf("				if !f.visit(node) {\n")
						w.Printf("					return;\n")
						w.Printf("				}\n")
					} else {
						w.Printf("				if !f.visit(&node.into()){\n")
						w.Printf("					return;\n")
						w.Printf("				}\n")
					}
					w.Printf("			}\n")
				} else if field.Type.IsPtr || field.Type.IsInterface {
					writeOnce()
					w.Printf("			if let Some(node) = &node.borrow().%s{\n", field.Name)
					if field.Type.Name == "Node" {
						w.Printf("				if !f.visit(node) {\n")
						w.Printf("					return;\n")
						w.Printf("				}\n")
					} else {
						w.Printf("				if !f.visit(&node.into()){\n")
						w.Printf("					return;\n")
						w.Printf("				}\n")
					}
					w.Printf("			}\n")
				}
			}
			if !once {
				w.Printf("		},\n")
			} else {
				w.Printf("		Node::%s(_)=>{},\n", d.Name)
			}
		}
	}
	w.Printf("	}\n")
	w.Printf("}\n\n")

}

func main() {
	if len(os.Args) != 2 {
		log.Println("usage: gen_ast2ts <file>")
		return
	}

	file := os.Args[1]

	log.SetFlags(log.Flags() | log.Lshortfile)

	list, err := gen.LoadFromDefaultSrc2JSON()

	if err != nil {
		log.Println(err)
		return
	}

	defs, err := gen.CollectDefinition(list, strcase.ToSnake, strcase.ToCamel, map[string]string{
		"uint":      "u64",
		"uintptr":   "usize",
		"string":    "String",
		"node_type": "String", // for raw node
		"any":       "HashMap<String,serde_json::Value>",
	})

	if err != nil {
		log.Println(err)
		return
	}

	if file == "/dev/stdout" {
		generate(os.Stdout, defs)
		return
	}

	file, err = filepath.Abs(file)

	if err != nil {
		log.Println(err)
		return
	}

	f, err := os.Create(file)

	if err != nil {
		log.Println(err)
		return
	}
	defer f.Close()

	generate(f, defs)

}
