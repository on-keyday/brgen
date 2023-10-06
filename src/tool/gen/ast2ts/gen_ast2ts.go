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
	for _, def := range defs.Defs {
		switch d := def.(type) {
		case *gen.Interface:
			w.Printf("export interface %s", d.Name)
			if d.Embed != "" {
				w.Printf(" extends %s", d.Embed)
			}
			w.Printf(" {\n")
			for _, field := range d.Fields {
				field.Type.IsPtr = false
				if field.Type.Name == "uint64" || field.Type.Name == "uintptr" {
					field.Type.Name = "number"
					w.Printf("	%s: number;\n", field.Name)
					continue
				}
				w.Printf("	%s: %s;\n", field.Name, field.Type)
			}
		case *gen.Struct:
			w.Printf("export interface %s ", d.Name)
			//commonFields := map[string]struct{}{}
			if len(d.Implements) > 0 {
				w.Printf("extends %s ", d.Implements[0])
			}
			w.Printf("{\n")
			for _, field := range d.Fields {
				field.Type.IsPtr = false
				if field.Type.Name == "uint64" || field.Type.Name == "uintptr" {
					field.Type.Name = "number"
					w.Printf("	%s: number;\n", field.Name)
					continue
				}
				w.Printf("	%s: %s;\n", field.Name, field.Type)
			}
			w.Printf("}\n\n")
		case *gen.Enum:
			w.Printf("export type %s = ", d.Name)
			for i, val := range d.Values {
				if i != 0 {
					w.Printf(" | ")
				}
				w.Printf("%q", val.Str)
			}
			w.Printf(";\n\n")
		}

	}
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

	defs, err := gen.CollectDefinition(list, strcase.ToSnake, strcase.ToCamel)

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
