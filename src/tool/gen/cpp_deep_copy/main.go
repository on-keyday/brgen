package main

import (
	"io"
	"log"
	"os"
	"path/filepath"

	"github.com/iancoleman/strcase"
	"github.com/on-keyday/brgen/src/tool/gen"
)

func generateDeepCopy(w *gen.Writer, defs *gen.Defs) {
	w.Printf("template<class T,class M>\n")
	w.Printf("std::shared_ptr<T> deep_copy(const std::shared_ptr<T>& node,M&& map);\n")
	w.Printf("\n")
	for _, def := range defs.Interfaces {
		w.Printf("template<class T,class M>\n")
		w.Printf("std::shared_ptr<T> deep_copy(const std::shared_ptr<%s>& node,M&& map){\n", def.Name)
		for _, derived := range def.Derived {
			found, ok := defs.Structs[derived]
			if !ok {
				continue
			}
			w.Printf("if(ast::as<%s>(node)) {\n", found.Name)
			w.Printf("return deep_copy(ast::cast_to<%s>(node),std::forward<M>(map));\n", found.Name)
			w.Printf("}\n")
		}
		w.Printf("return nullptr;\n")
		w.Printf("}\n")
	}
	for _, def := range defs.Structs {
		if len(def.Implements) == 0 {
			continue
		}
		w.Printf("template<class T,class M>\n")
		w.Printf("std::shared_ptr<T> deep_copy(const std::shared_ptr<%s>& node,M&& map){\n", def.Name)
		w.Printf("auto new_node=std::make_shared<%s>();\n", def.Name)
		for _, field := range def.Fields {
			if field.Type.IsArray {
				w.Printf("for(auto& i:node->%s){\n", field.Tag)
				if field.Type.IsWeak {
					w.Printf("new_node->%s.push_back(deep_copy(i.lock(),map));\n", field.Tag)
				} else {
					w.Printf("new_node->%s.push_back(deep_copy(i,map));\n", field.Tag)
				}
			} else if field.Type.IsPtr || field.Type.IsInterface {
				if field.Type.IsWeak {
					w.Printf("if(auto it = map.find(node->%s.lock());it !=map.end()){\n", field.Tag)
					w.Printf("new_node->%s= ast::cast_to<%s>(it->second);\n", field.Tag, field.Type.Name)
					w.Printf("}else{\n")
					w.Printf("new_node->%s=deep_copy(node->%s.lock(),map);\n", field.Tag, field.Tag)
					w.Printf("}\n")
				} else {
					w.Printf("if(auto it = map.find(node->%s);it !=map.end()){\n", field.Tag)
					w.Printf("new_node->%s= ast::cast_to<%s>(it->second);\n", field.Tag, field.Type.Name)
					w.Printf("}else{\n")
					w.Printf("new_node->%s=deep_copy(node->%s,map);\n", field.Tag, field.Tag)
					w.Printf("}\n")
				}
			} else {
				w.Printf("new_node->%s=node->%s;\n", field.Tag, field.Tag)
			}
		}
		w.Printf("return new_node;\n")
		w.Printf("}\n")
	}
}

func generate(rw io.Writer, defs *gen.Defs) {
	w := gen.NewWriter(rw)
	w.Printf("// Code generated by gen_ast2cpp; DO NOT EDIT.\n\n")
	w.Printf("#pragma once\n\n")
	w.Printf("#include <core/ast/ast.h>\n\n")
	w.Printf("namespace brgen::ast {\n\n")
	generateDeepCopy(w, defs)
	w.Printf("}\n")
}

func main() {
	if len(os.Args) != 2 {
		log.Println("usage: gen_cpp_deep_copy <file>")
		return
	}

	file := os.Args[1]

	log.SetFlags(log.Flags() | log.Lshortfile)

	list, err := gen.LoadFromDefaultSrc2JSON()

	if err != nil {
		log.Println(err)
		return
	}

	defs, err := gen.CollectDefinition(list, func(s string) string { return s }, strcase.ToCamel, map[string]string{})
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
