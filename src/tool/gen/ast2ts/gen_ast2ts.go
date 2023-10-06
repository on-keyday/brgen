package main

import (
	"io"
	"log"
	"os"
	"path/filepath"

	"github.com/on-keyday/brgen/src/tool/gen"
)

func generate(rw io.Writer, defs []gen.Def) {
	w := gen.NewWriter(rw)
	for _, def := range defs {
		switch d := def.(type) {
		case *gen.Struct:
			w.Printf("export interface %s {\n", d.Name)
			for _, field := range d.Fields {
				field.Type.IsPtr = false
				if field.Type.Name == "uint" || field.Type.Name == "uintptr" {
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

	defs, err := gen.CollectDefinition(list)

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
