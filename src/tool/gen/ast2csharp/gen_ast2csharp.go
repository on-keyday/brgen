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
	w.Printf("using System;\n")
	w.Printf("using System.CodeDom.Compiler;\n")
	w.Printf("using System.Collections.Generic;\n")

	w.Printf("namespace ast2cs {\n")

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
			w.Printf("public class %s ", v.Name)
			if len(v.Implements) > 0 {
				w.Printf(": %s", v.Implements[0])
			}
			w.Printf("{\n")
			for _, f := range v.Fields {
				if f.Name == v.Name {
					f.Name += "_" // avoid name conflict
				}
				w.Printf("	public %s %s{get;set;}\n", f.Type.CSharpString(), f.Name)
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
		"any":     "Dictionary<string,object>",
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
