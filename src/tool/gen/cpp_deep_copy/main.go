package main

import (
	"io"
	"log"
	"os"
	"path/filepath"

	"github.com/iancoleman/strcase"
	"github.com/on-keyday/brgen/src/tool/gen"
)

func generateSingleDeepEqual(w *gen.Writer, def *gen.Struct) {
	w.Printf("template<class NodeM,class ScopeM,class BackTracer=NullBackTracer>\n")
	w.Printf("constexpr bool deep_equal(const std::shared_ptr<%s>& a,const std::shared_ptr<%s>& b,NodeM&& node_map,ScopeM&& scope_map,BackTracer&& trace = BackTracer{}){\n", def.Name, def.Name)
	w.Printf("if(!a && !b) return true;\n")
	w.Printf("if(!a || !b) return false;\n")
	if def.Name == "Scope" {
		w.Printf("scope_map[a]=b;\n")
	} else {
		w.Printf("node_map[a]=b;\n")
	}
	for _, field := range def.Fields {
		if field.Type.IsArray {
			tag := field.Tag
			if def.Name == "Scope" && field.Tag == "ident" { // special edge case
				tag = "objects"
			}
			nodeAccessA := "a->" + tag + "[i]"
			nodeAccessB := "b->" + tag + "[i]"
			if field.Type.IsWeak {
				nodeAccessA = nodeAccessA + ".lock()"
				nodeAccessB = nodeAccessB + ".lock()"
			}
			w.Printf("if(a->%s.size()!=b->%s.size()) return false;\n", tag, tag)
			w.Printf("for(size_t i=0;i<a->%s.size();i++){\n", tag)
			w.Printf("if(!deep_equal(%s,%s,std::forward<NodeM>(node_map),std::forward<ScopeM>(scope_map),std::forward<BackTracer>(trace))) {\n", nodeAccessA, nodeAccessB)
			w.Printf("trace(%s,%s,\"%s::%s\",i);\n", nodeAccessA, nodeAccessB, field.Type.Name, tag)
			w.Printf("return false;\n")
			w.Printf("}\n")
			w.Printf("}\n")
		} else if field.Type.IsPtr || field.Type.IsInterface {
			mapName := "node_map"
			if field.Type.Name == "Scope" {
				mapName = "scope_map"
			}
			nodeAccessA := "a->" + field.Tag
			nodeAccessB := "b->" + field.Tag
			if field.Type.IsWeak {
				nodeAccessA = nodeAccessA + ".lock()"
				nodeAccessB = nodeAccessB + ".lock()"
			}
			w.Printf("if(auto it = %[1]s.find(%[2]s);it !=%[1]s.end()){\n", mapName, nodeAccessA)
			if mapName == "scope_map" {
				w.Printf("if(it->second!=%s) {\n", nodeAccessB)
				w.Printf("trace(%s,%s,\"%s::%s\",-1);\n", nodeAccessA, nodeAccessB, field.Type.Name, field.Tag)
				w.Printf("return false;\n")
				w.Printf("}\n")
				w.Printf("}else{\n")
				w.Printf("if(!deep_equal(%s,%s,std::forward<NodeM>(node_map),std::forward<ScopeM>(scope_map),std::forward<BackTracer>(trace))) {\n", nodeAccessA, nodeAccessB)
				w.Printf("trace(%s,%s,\"%s::%s\",-1);\n", nodeAccessA, nodeAccessB, field.Type.Name, field.Tag)
				w.Printf("return false;\n")
				w.Printf("}\n")
			} else {
				w.Printf("if(ast::cast_to<%s>(it->second)!=%s) {\n", field.Type.Name, nodeAccessB)
				w.Printf("trace(%s,%s,\"%s::%s\",-1);\n", nodeAccessA, nodeAccessB, field.Type.Name, field.Tag)
				w.Printf("return false;\n")
				w.Printf("}\n")
				w.Printf("}else{\n")
				w.Printf("if(!deep_equal(%s,%s,std::forward<NodeM>(node_map),std::forward<ScopeM>(scope_map),std::forward<BackTracer>(trace))) {\n", nodeAccessA, nodeAccessB)
				w.Printf("trace(%s,%s,\"%s::%s\",-1);\n", nodeAccessA, nodeAccessB, field.Type.Name, field.Tag)
				w.Printf("return false;\n")
				w.Printf("}\n")
			}
			w.Printf("}\n")
		} else {
			w.Printf("if(a->%s!=b->%s) {\n", field.Tag, field.Tag)
			w.Printf("trace(a->%s,b->%s,\"%s::%s\",-1);\n", field.Tag, field.Tag, def.Name, field.Tag)
			w.Printf("return false;\n")
			w.Printf("}\n")
		}
	}
	w.Printf("return true;\n")
	w.Printf("}\n")
}

func generateDeepEqual(w *gen.Writer, defs *gen.Defs) {
	w.Printf("template<class T,class NodeM,class ScopeM,class BackTracer=NullBackTracer>\n")
	w.Printf("constexpr bool deep_equal(const std::shared_ptr<T>& a,const std::shared_ptr<T>& b,NodeM&& node_map,ScopeM&& scope_map,BackTracer&& trace=BackTracer{});\n")
	w.Printf("\n")
	for _, v := range defs.Defs {
		switch def := v.(type) {
		case *gen.Interface:
			w.Printf("template<class NodeM,class ScopeM,class BackTracer=NullBackTracer>\n")
			w.Printf("constexpr bool deep_equal(const std::shared_ptr<%s>& a,const std::shared_ptr<%s>& b,NodeM&& node_map,ScopeM&& scope_map,BackTracer&& trace=BackTracer{}){\n", def.Name, def.Name)
			for _, derived := range def.Derived {
				found, ok := defs.Structs[derived]
				if !ok {
					continue
				}
				w.Printf("if(ast::as<%s>(a)) {\n", found.Name)
				w.Printf("if(!ast::as<%s>(b)) return false;\n", found.Name)
				w.Printf("return deep_equal(ast::cast_to<%s>(a),ast::cast_to<%s>(b),std::forward<NodeM>(node_map),std::forward<ScopeM>(scope_map),std::forward<BackTracer>(trace));\n", found.Name, found.Name)
				w.Printf("}\n")
			}
			w.Printf("return false;\n")
			w.Printf("}\n")
		case *gen.Struct:
			if len(def.Implements) == 0 {
				if def.Name == "Scope" {
					generateSingleDeepEqual(w, def)
				}
				continue
			}
			generateSingleDeepEqual(w, def)
		}
	}
}

func generateSingleDeepCopy(w *gen.Writer, def *gen.Struct) {
	w.Printf("template<class NodeM,class ScopeM>\n")
	w.Printf("std::shared_ptr<%s> deep_copy(const std::shared_ptr<%s>& node,NodeM&& node_map,ScopeM&& scope_map){\n", def.Name, def.Name)
	w.Printf("if(!node){\n")
	w.Printf("return nullptr;\n")
	w.Printf("}\n")
	w.Printf("auto new_node=std::make_shared<%s>();\n", def.Name)
	if def.Name == "Scope" {
		w.Printf("scope_map[node]=new_node;\n")
	} else {
		w.Printf("node_map[node]=new_node;\n")
	}
	for _, field := range def.Fields {
		if field.Type.IsArray {
			tag := field.Tag
			if def.Name == "Scope" && field.Tag == "ident" { // special edge case
				tag = "objects"
			}
			w.Printf("for(auto& i:node->%s){\n", tag)
			if field.Type.IsWeak {
				w.Printf("new_node->%s.push_back(deep_copy(i.lock(),std::forward<NodeM>(node_map),std::forward<ScopeM>(scope_map)));\n", tag)
			} else {
				w.Printf("new_node->%s.push_back(deep_copy(i,std::forward<NodeM>(node_map),std::forward<ScopeM>(scope_map)));\n", tag)
			}
			w.Printf("}\n")
		} else if field.Type.IsPtr || field.Type.IsInterface {
			mapName := "node_map"
			if field.Type.Name == "Scope" {
				mapName = "scope_map"
			}
			nodeAccess := "node->" + field.Tag
			if field.Type.IsWeak {
				nodeAccess = nodeAccess + ".lock()"
			}
			w.Printf("if(auto it = %[1]s.find(%[2]s);it !=%[1]s.end()){\n", mapName, nodeAccess)
			if mapName == "scope_map" {
				w.Printf("new_node->%s=it->second;\n", field.Tag)
			} else {
				w.Printf("new_node->%s= ast::cast_to<%s>(it->second);\n", field.Tag, field.Type.Name)
			}
			w.Printf("}else{\n")
			w.Printf("new_node->%s=deep_copy(%s,std::forward<NodeM>(node_map),std::forward<ScopeM>(scope_map));\n", field.Tag, nodeAccess)
			w.Printf("}\n")
		} else {
			w.Printf("new_node->%s=node->%s;\n", field.Tag, field.Tag)
		}
	}
	w.Printf("return new_node;\n")
	w.Printf("}\n")
}

func generateDeepCopy(w *gen.Writer, defs *gen.Defs) {
	w.Printf("template<class T,class NodeM,class ScopeM>\n")
	w.Printf("std::shared_ptr<T> deep_copy(const std::shared_ptr<T>& node,NodeM&& node_map,ScopeM&& scope_map);\n")
	w.Printf("\n")
	for _, v := range defs.Defs {
		switch def := v.(type) {
		case *gen.Interface:
			w.Printf("template<class NodeM,class ScopeM>\n")
			w.Printf("std::shared_ptr<%s> deep_copy(const std::shared_ptr<%s>& node,NodeM&& node_map,ScopeM&& scope_map){\n", def.Name, def.Name)
			for _, derived := range def.Derived {
				found, ok := defs.Structs[derived]
				if !ok {
					continue
				}
				w.Printf("if(ast::as<%s>(node)) {\n", found.Name)
				w.Printf("return deep_copy(ast::cast_to<%s>(node),std::forward<NodeM>(node_map),std::forward<ScopeM>(scope_map));\n", found.Name)
				w.Printf("}\n")
			}
			w.Printf("return nullptr;\n")
			w.Printf("}\n")
		case *gen.Struct:
			if len(def.Implements) == 0 {
				if def.Name == "Scope" {
					generateSingleDeepCopy(w, def)
				}
				continue
			}
			generateSingleDeepCopy(w, def)
		default:
		}
	}
	w.Printf("\n")

}

func generateTest(w *gen.Writer) {
	w.Printf("namespace test {\n")
	w.Printf("template<class NodeM,class ScopeM>\n")
	w.Printf("inline bool test_single_deep_copy(const std::shared_ptr<Node>& n) {\n")
	w.Printf("const auto copy = deep_copy(n,NodeM{},ScopeM{});\n")
	w.Printf("return deep_equal(n,copy,NodeM{},ScopeM{});\n")
	w.Printf("}\n")
	w.Printf("}\n")
}

func generateBackTracer(w *gen.Writer) {
	w.Printf("struct NullBackTracer{\n")
	w.Printf("constexpr void operator()(auto&&a,auto&& b,const char* which,size_t index){}\n")
	w.Printf("};\n")
}

func generate(rw io.Writer, defs *gen.Defs) {
	w := gen.NewWriter(rw)
	w.Printf("// Code generated by gen_cpp_deep_copy; DO NOT EDIT.\n\n")
	w.Printf("#pragma once\n\n")
	w.Printf("#include <core/ast/ast.h>\n\n")
	w.Printf("namespace brgen::ast {\n\n")
	generateBackTracer(w)
	generateDeepCopy(w, defs)
	generateDeepEqual(w, defs)
	generateTest(w)
	w.Printf("}\n")
}

func main() {
	if len(os.Args) < 2 {
		log.Println("usage: gen_cpp_deep_copy <file> [test]")
		return
	}

	file := os.Args[1]

	log.SetFlags(log.Flags() | log.Lshortfile)

	list, err := gen.LoadFromDefaultSrc2JSON()

	if err != nil {
		log.Println(err)
		return
	}

	defs, err := gen.CollectDefinition(list, func(s string) string { return s }, func(s string) string {
		p := strcase.ToCamel(s)
		if p == "IoOperation" {
			return "IOOperation"
		}
		return p
	}, map[string]string{})
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
