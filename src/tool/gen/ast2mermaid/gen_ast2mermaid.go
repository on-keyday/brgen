package main

import (
	"flag"
	"io"
	"log"
	"os"
	"path/filepath"

	"github.com/iancoleman/strcase"
	"github.com/on-keyday/brgen/src/tool/gen"
)

func generateFlow(rw io.Writer, d *gen.Defs) {
	w := gen.NewWriter(rw)
	//w.Printf("```mermaid\n")
	//defer w.Printf("```\n")
	w.Printf("flowchart TB\n")
	for _, def := range d.Defs {
		switch val := def.(type) {
		case *gen.Interface:
			w.Printf("%s[%s]\n", val.Name, val.Name)
			if len(val.Embed) > 0 {
				w.Printf("%s -->|derive|%s\n", val.Embed, val.Name)
			}
			for _, m := range val.UnCommonFields {
				w.Printf("%s -->|member|%s\n", val.Name, m.Name)
				w.Printf("%s -->|type|%s\n", m.Name, m.Type.Name)
			}
		case *gen.Struct:
			w.Printf("%s[%s]\n", val.Name, val.Name)
			if len(val.Implements) > 0 {
				w.Printf("%s -->|derive|%s\n", val.Implements[0], val.Name)
			}
			for _, m := range val.UnCommonFields {
				w.Printf("%s -->|member|%s\n", val.Name, m.Name)
				w.Printf("%s -->|type|%s\n", m.Name, m.Type.Name)
			}
		case *gen.Enum:
			w.Printf("%s[%s]\n", val.Name, val.Name)
			for _, m := range val.Values {
				if m.Name == "call" {
					m.Name = "call_"
				}
				if m.Name == "end" {
					m.Name = "end_"
				}
				w.Printf("%s -->|member|%s\n", val.Name, m.Name)
			}
		}
	}
}

func generateEr(rw io.Writer, d *gen.Defs) {
	w := gen.NewWriter(rw)
	//w.Printf("```mermaid\n")
	//defer w.Printf("```\n")
	w.Printf("erDiagram\n")
	for _, def := range d.Defs {
		switch val := def.(type) {
		case *gen.Interface:
			w.Printf("%s {\n", val.Name)
			for _, m := range val.UnCommonFields {
				if m.Type.IsArray {
					w.Printf("%s[] %s\n", m.Type.Name, m.Name)
				} else {
					w.Printf("%s %s\n", m.Type.Name, m.Name)
				}
			}
			w.Printf("}\n")
			if len(val.Embed) > 0 {
				w.Printf("%s |o--|| %s : derive\n", val.Embed, val.Name)
			}
			for _, m := range val.UnCommonFields {
				if m.Type.IsWeak {
					w.Printf("%s |o--|| %s : weak\n", val.Name, m.Type.Name)
				} else {
					w.Printf("%s |o--|| %s : strong\n", val.Name, m.Type.Name)
				}
			}
		case *gen.Struct:
			if len(val.Implements) > 0 {
				w.Printf("%s |o--|| %s : derive\n", val.Implements[0], val.Name)
			}
			w.Printf("%s {\n", val.Name)
			for _, m := range val.UnCommonFields {
				if m.Type.IsArray {
					w.Printf("%s[] %s\n", m.Type.Name, m.Name)
				} else {
					w.Printf("%s %s\n", m.Type.Name, m.Name)
				}
			}
			w.Printf("}\n")
			for _, m := range val.UnCommonFields {
				if m.Type.IsWeak {
					w.Printf("%s |o--||%s : weak\n", val.Name, m.Type.Name)
				} else {
					w.Printf("%s |o--||%s : strong\n", val.Name, m.Type.Name)
				}
			}
		case *gen.Enum:
			w.Printf("%s {\n", val.Name)
			for _, m := range val.Values {
				if m.Name == "call" {
					m.Name = "call_"
				}
				if m.Name == "end" {
					m.Name = "end_"
				}
				w.Printf("%s %s\n", val.Name, m.Name)
			}
			w.Printf("}\n")
		}
	}
}

var flow = flag.Bool("flow", false, "generate flowchart")

func main() {
	flag.Parse()
	if len(flag.Args()) != 1 {
		log.Println("usage: gen_ast2mermaid <file>")
		return
	}

	file := flag.Arg(0)

	log.SetFlags(log.Flags() | log.Lshortfile)

	list, err := gen.LoadFromDefaultSrc2JSON()

	if err != nil {
		log.Println(err)
		return
	}

	defs, err := gen.CollectDefinition(list, strcase.ToSnake, strcase.ToCamel, map[string]string{
		"uint":    "number",
		"uintptr": "number",
		"bool":    "boolean",
	})

	if err != nil {
		log.Println(err)
		return
	}

	if file == "/dev/stdout" || file == "stdout" {
		if *flow {
			generateFlow(os.Stdout, defs)
		} else {
			generateEr(os.Stdout, defs)
		}
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

	if *flow {
		generateFlow(f, defs)
	} else {
		generateEr(f, defs)
	}

}
