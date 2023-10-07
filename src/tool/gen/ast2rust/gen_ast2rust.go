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

	for _, def := range defs.Defs {
		switch d := def.(type) {
		case *gen.Struct:
			w.Printf("pub struct %s {\n", d.Name)
			for _, field := range d.Fields {
				w.Printf("pub %s: %s,\n", field.Name, field.Type)
			}
			w.Printf("}\n\n")
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

	defs, err := gen.CollectDefinition(list, strcase.ToSnake, strcase.ToCamel, map[string]string{
		"uint":    "u64",
		"uintptr": "usize",
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
