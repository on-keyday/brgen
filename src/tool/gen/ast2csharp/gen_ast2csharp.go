package main

import (
	"log"
	"os"
	"path/filepath"

	"github.com/iancoleman/strcase"
	"github.com/on-keyday/brgen/src/tool/gen"
)

func generate(f *os.File, defs *gen.Defs) {
	w := gen.NewWriter(f)
	w.Printf("// Code generated by gen_ast2csharp.go. DO NOT EDIT.\n")
	w.Printf("using System;\n")
	w.Printf("using System.CodeDom.Compiler;\n")
	w.Printf("using System.Collections.Generic;\n")

	w.Printf("namespace ast2cs {\n")
	defer w.Printf("}\n")

	for _, v := range defs.Defs {
		switch v := v.(type) {
		case *gen.Interface:
			w.Printf("public interface %s ", v.Name)
			if v.Embed != "" {
				w.Printf(": %s ", v.Embed)
			}
			w.Printf("{\n")
			for _, v := range v.UnCommonFields {
				w.Printf("	public %s %s {get; set;}\n", v.Type.CSharpString(), v.Name)
			}
			w.Printf("}\n")

		case *gen.Struct:
			if v.Name == "Loc" || v.Name == "Pos" {
				w.Printf("public struct %s ", v.Name)
			} else {
				w.Printf("public class %s ", v.Name)
			}
			if len(v.Implements) > 0 {
				w.Printf(": %s", v.Implements[0])
			}
			w.Printf("{\n")
			for _, f := range v.Fields {
				if f.Name == v.Name {
					f.Name += "_" // avoid name conflict
				}
				typStr := f.Type.CSharpString()
				if typStr == "string" {
					w.Printf("	public %s %s{get;set;} = \"\";\n", typStr, f.Name)
				} else {
					w.Printf("	public %s %s{get;set;}\n", typStr, f.Name)
				}
			}
			w.Printf("}\n")
		case *gen.Enum:
			w.Printf("public enum %s {\n", v.Name)
			for _, v := range v.Values {
				w.Printf("%s,\n", v.Name)
			}
			w.Printf("}\n")
		}
	}
	w.Printf("public static class Ast {\n")
	w.Printf("  public static Program ParseAST(JsonAst ast) {\n")
	w.Printf("       if(ast.Node == null) {\n")
	w.Printf("           throw new NullReferenceException(\"ast.Node is null\");\n")
	w.Printf("       }\n")
	w.Printf("       if(ast.Scope == null) {\n")
	w.Printf("           throw new NullReferenceException(\"ast.Scope is null\");\n")
	w.Printf("       }\n")
	w.Printf("       var nodes = new Node[ast.Node.Count];\n")
	w.Printf("       var scopes = new Scope[ast.Scope.Count];\n")
	w.Printf("       for (int i = 0; i < ast.Node.Count; i++) {\n")
	w.Printf("           switch (ast.Node[i].NodeType) {\n")
	for _, v := range defs.Defs {
		switch v := v.(type) {
		case *gen.Struct:
			if len(v.Implements) == 0 {
				continue
			}
			w.Printf("           case NodeType.%s:\n", v.Name)
			w.Printf("               nodes[i] = new %s() { Loc = ast.Node[i].Loc };\n", v.Name)
			w.Printf("               break;\n")
		}
	}
	w.Printf("           }\n")
	w.Printf("       }\n")
	w.Printf("       for (int i = 0; i < ast.Scope.Count; i++) {\n")
	w.Printf("           scopes[i] = new Scope();\n")
	w.Printf("       }\n")
	w.Printf("       for (int i = 0; i < ast.Node.Count; i++) {\n")
	w.Printf("           switch (ast.Node[i].NodeType) {\n")
	for _, v := range defs.Defs {
		switch v := v.(type) {
		case *gen.Struct:
			if len(v.Implements) == 0 {
				continue
			}
			w.Printf("           case NodeType.%s:\n", v.Name)
			w.Printf("               var node = nodes[i] as %s;\n", v.Name)
			for _, f := range v.Fields {
				w.Printf("               node.%s = ast.Node[i].Body[%s];\n", f.Name, f.Tag)
			}
		}
	}
	w.Printf("  }\n")
	w.Printf("}\n")
}

func main() {
	if len(os.Args) != 2 {
		log.Println("usage: gen_ast2go <file>")
		return
	}

	file := os.Args[1]

	log.SetFlags(log.Flags() | log.Lshortfile)

	list, err := gen.LoadFromDefaultSrc2JSON()

	if err != nil {
		log.Println(err)
		return
	}

	defs, err := gen.CollectDefinition(list, func(s string) string {
		return strcase.ToCamel(s)
	}, strcase.ToCamel, map[string]string{
		"uint":    "ulong",
		"uintptr": "ulong",
		"any":     "Dictionary<string,object>?",
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
