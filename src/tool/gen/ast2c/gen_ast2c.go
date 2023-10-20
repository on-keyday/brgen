package main

import (
	"io"
	"log"
	"os"
	"path/filepath"
	"strings"

	"github.com/iancoleman/strcase"
	"github.com/on-keyday/brgen/src/tool/gen"
)

func generateHeader(rw io.Writer, defs *gen.Defs, single bool) {
	w := gen.NewWriter(rw)
	if !single {
		w.Printf("#pragma once\n#ifndef __AST_H__\n#define __AST_H__\n\n")
	}
	w.Printf("#include <stdint.h>\n")
	/*
		this include imports definition below
		// sync with json_stub.h

		typedef struct json_handlers json_handlers;
		struct json_handlers {
		    void* ctx;
		    void* (*object_get)(json_handlers*, void*, const char* name);
		    void* (*array_get)(json_handlers*, void*, size_t i);
		    size_t (*array_size)(json_handlers*, void*);
		    char* (*string_get_alloc)(json_handlers*, void*);
		    // returns non-zero for success, 0 for error
		    int (*number_get)(json_handlers*, void*, uint64_t* n);
		    // returns 0 or 1. -1 for error
		    int (*boolean_get)(json_handlers*, void*);

		    void* (*alloc)(json_handlers*, size_t size, size_t align);
		};

	*/
	w.Printf("#include %q\n\n", "json_stub.h")
	// define node types
	w.Printf("typedef enum NodeType {\n")
	for _, typ := range defs.NodeTypes {
		w.Printf("\t%s,\n", strings.ToUpper(typ))
	}
	w.Printf("} NodeType;\n\n")
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
				w.Printf("\t%s,\n", strings.ToUpper(field.Name))
			}
			w.Printf("};\n\n")

		}
	}
	// next pointer types
	for _, def := range defs.Defs {
		switch d := def.(type) {
		case *gen.Interface:
			w.Printf("struct %s {\n", d.Name)
			w.Printf("\tconst NodeType node_type;\n")
			for _, field := range d.Fields {
				w.Printf("\t%s %s;\n", field.Type.CString(), field.Name)
			}
			w.Printf("};\n\n")
		case *gen.Struct:
			if len(d.Implements) == 0 && d.Name != "Scope" {
				continue
			}
			w.Printf("struct %s {\n", d.Name)
			w.Printf("\tconst NodeType node_type;\n")
			for _, field := range d.Fields {
				w.Printf("\t%s %s;\n", field.Type.CString(), field.Name)
			}
			w.Printf("};\n\n")
		}
	}
	if !single {
		w.Printf("#endif\n")
	}
}

func generateSource(rw io.Writer, defs *gen.Defs, single bool) {
	w := gen.NewWriter(rw)
	if !single {
		w.Printf("#include \"ast.h\"\n\n")
	}

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
		"uintptr": "uint64_t",
		"bool":    "int",
		"string":  "char*",
	})

	if err != nil {
		log.Println(err)
		return
	}

	if hdr == "/dev/stdout" {
		generateHeader(os.Stdout, defs, false)
		generateSource(os.Stdout, defs, false)
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
		generateSource(f, defs, true)
		return
	}

	f2, err := os.Create(src)
	if err != nil {
		log.Println(err)
		return
	}
	defer f2.Close()

	generateHeader(f, defs, false)
	generateSource(f2, defs, false)

}
