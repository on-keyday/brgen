package main

import (
	"io"
	"log"
	"os"
	"path/filepath"

	"github.com/iancoleman/strcase"
	"github.com/on-keyday/brgen/src/tool/gen"
)

func generate(w io.Writer, list *gen.Defs) {
	g := gen.NewWriter(w)
	g.Printf("library ast;\n")

	writeField := func(f *gen.Field) {
		typ := f.Type.DartString()
		g.Printf("%s %s", typ, f.Name)
		if f.Type.IsArray {
			g.Printf(" = []")
		} else if f.Type.Name == "String" {
			g.Printf(" = ''")
		} else if !f.Type.IsOptional && !f.Type.IsArray && f.Type.Name == "int" {
			g.Printf(" = 0")
		} else if f.Type.Name == "bool" {
			g.Printf(" = false")
		} else if t, ok := list.Enums[f.Type.Name]; ok {
			g.Printf(" = %s.%s", t.Name, t.Values[0].Name)
		} else if f.Type.Name == "Loc" || f.Type.Name == "Pos" {
			g.Printf(" = %s()", f.Type.Name)
		}
		g.Printf(";\n")

	}

	for _, def := range list.Defs {
		switch v := def.(type) {
		case *gen.Interface:
			g.Printf("abstract class %s ", v.Name)
			if v.Embed != "" {
				g.Printf("extends %s ", v.Embed)
			}
			g.Printf("{\n")
			for _, e := range v.UnCommonFields {
				writeField(e)
			}
			g.Printf("}\n")
		case *gen.Struct:
			g.Printf("class %s ", v.Name)
			if len(v.Implements) > 0 {
				g.Printf("extends %s ", v.Implements[0])
			}
			g.Printf("{\n")
			for _, e := range v.UnCommonFields {
				writeField(e)
			}
			g.Printf("}\n")
		case *gen.Enum:
			g.Printf("enum %s {\n", v.Name)
			for _, e := range v.Values {
				e.Name = strcase.ToCamel(e.Name)
				g.Printf("%s,\n", e.Name)
			}
			g.Printf("}\n")
		}
	}
}

func main() {
	if len(os.Args) != 2 {
		log.Println("usage: gen_ast2dart <file>")
		return
	}

	file := os.Args[1]

	log.SetFlags(log.Flags() | log.Lshortfile)

	list, err := gen.LoadFromDefaultSrc2JSON()

	if err != nil {
		log.Println(err)
		return
	}

	defs, err := gen.CollectDefinition(list, strcase.ToLowerCamel, func(s string) string {
		if s == "function" {
			return "Func"
		}
		return strcase.ToCamel(s)
	}, map[string]string{
		"uint":    "int",
		"any":     "dynamic",
		"uintptr": "int",
		"string":  "String",
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
