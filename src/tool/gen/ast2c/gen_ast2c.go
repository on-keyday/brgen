package main

import (
	"io"
	"log"
	"os"
	"path/filepath"

	"github.com/iancoleman/strcase"
	"github.com/on-keyday/brgen/src/tool/gen"
)

func generateHeader(rw io.Writer, defs *gen.Defs, single bool) {
	w := gen.NewWriter(rw)
	if !single {
		w.Printf("#pragma once\n#ifndef __AST_H__\n#define __AST_H__\n\n")
	}
	w.Printf("#include <stdint.h>\n\n")
	// declare all struct types
	for _, def := range defs.Defs {
		switch d := def.(type) {
		case *gen.Interface:
			w.Printf("typedef struct %s %s;\n", d.Name, d.Name)
		case *gen.Struct:
			w.Printf("typedef struct %s %s;\n", d.Name, d.Name)
		case *gen.Enum:
			w.Printf("typedef enum %s %s;\n", d.Name, d.Name)
		}
	}
	// next non pointer or interface types
	for _, def := range defs.Defs {
		switch d := def.(type) {
		case *gen.Interface:
			continue
		case *gen.Struct:
			if len(d.Implements) == 0 && d.Name != "Scope" {
				w.Printf("struct %s {\n", d.Name)
				for _, field := range d.Fields {
					w.Printf("\t%s %s;\n", field.Type.CString(), field.Name)
				}
				w.Printf("};\n\n")
			}
		case *gen.Enum:
			w.Printf("enum %s {\n", d.Name)
			for _, field := range d.Values {
				w.Printf("\t%s,\n", field.Name)
			}
			w.Printf("};\n\n")

		}
	}
	// next pointer types
	for _, def := range defs.Defs {
		switch d := def.(type) {
		case *gen.Interface:
			w.Printf("struct %s {\n", d.Name)
			for _, field := range d.Fields {
				w.Printf("\t%s %s;\n", field.Type.CString(), field.Name)
			}
			w.Printf("};\n\n")
		case *gen.Struct:
			if len(d.Implements) == 0 && d.Name != "Scope" {
				continue
			}
			w.Printf("struct %s {\n", d.Name)
			for _, field := range d.Fields {
				w.Printf("\t%s %s;\n", field.Type.CString(), field.Name)
			}
			w.Printf("};\n\n")
		}
	}
	w.Printf("#endif\n")
}

func generateSource(rw io.Writer, defs *gen.Defs) {
	w := gen.NewWriter(rw)
	w.Printf("#include \"ast.h\"\n\n")
}

func main() {
	if len(os.Args) != 2 && len(os.Args) != 3 {
		log.Println("usage: gen_ast2c [<header file> <source file>] | /dev/stdout")
		return
	}

	hdr := os.Args[1]
	var src string
	if len(os.Args) == 3 {
		src = os.Args[2]
	} else {
		src = hdr
	}

	log.SetFlags(log.Flags() | log.Lshortfile)

	list, err := gen.LoadFromDefaultSrc2JSON()

	if err != nil {
		log.Println(err)
		return
	}

	defs, err := gen.CollectDefinition(list, strcase.ToSnake, strcase.ToCamel, map[string]string{
		"uint":    "uint64_t",
		"uintptr": "uintptr_t",
		"bool":    "int",
		"string":  "char*",
	})

	if err != nil {
		log.Println(err)
		return
	}

	if hdr == "/dev/stdout" {
		generateHeader(os.Stdout, defs, false)
		generateSource(os.Stdout, defs)
		return
	}

	hdr, err = filepath.Abs(hdr)

	if err != nil {
		log.Println(err)
		return
	}

	src, err = filepath.Abs(src)
	if err != nil {
		log.Println(err)
		return
	}

	f, err := os.Create(hdr)

	if err != nil {
		log.Println(err)
		return
	}
	defer f.Close()

	if hdr == src {
		generateHeader(f, defs, true)
		generateSource(f, defs)
		return
	}

	f2, err := os.Create(src)
	if err != nil {
		log.Println(err)
		return
	}
	defer f2.Close()

	generateHeader(f, defs, false)
	generateSource(f2, defs)

}
