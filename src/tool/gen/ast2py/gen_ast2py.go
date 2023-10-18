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

func generate(rw io.Writer, defs *gen.Defs) {
	w := gen.NewWriter(rw)
	w.Printf("from __future__ import annotations\n\n")
	w.Printf("from typing import Optional,List\n\n")
	w.Printf("from enum import Enum\n\n")

	for _, def := range defs.Defs {
		switch v := def.(type) {
		case *gen.Interface:
			w.Printf("class %s", v.Name)
			commonFields := map[string]struct{}{}
			if v.Embed != "" {
				w.Printf("(%s)", v.Embed)
				embed := v.Embed
				for embed != "" {
					iface := defs.Interfaces[embed]
					for _, field := range iface.Fields {
						commonFields[field.Name] = struct{}{}
					}
					embed = iface.Embed
				}
			}
			w.Printf(":\n")
			count := 0
			for _, field := range v.Fields {
				if _, ok := commonFields[field.Name]; ok {
					continue
				}
				w.Printf("	%s: %s\n", field.Name, field.Type.PyString())
				count++
			}
			if count == 0 {
				w.Printf("\tpass\n")
			}
		case *gen.Struct:
			w.Printf("class %s", v.Name)
			commonFields := map[string]struct{}{}
			if len(v.Implements) > 0 {
				w.Printf("(%s)", v.Implements[0])
				for _, iface := range v.Implements {
					for _, field := range defs.Interfaces[iface].Fields {
						commonFields[field.Name] = struct{}{}
					}
				}
			}
			w.Printf(":\n")
			count := 0
			for _, field := range v.Fields {
				if _, ok := commonFields[field.Name]; ok {
					continue
				}
				w.Printf("\t%s: %s\n", field.Name, field.Type.PyString())
				count++
			}
			if count == 0 {
				w.Printf("\tpass\n")
			}

		case *gen.Enum:
			w.Printf("class %s(Enum):\n", v.Name)
			for _, field := range v.Values {
				w.Printf("\t%s = %q\n", strings.ToUpper(field.Name), field.Str)
			}
		}
		w.Printf("\n\n")
	}

	w.Printf("class RawNode:\n")
	w.Printf("\tnode_type: str\n")
	w.Printf("\tloc: Loc\n")
	w.Printf("\tbody: Dict[str,Any]\n\n")

	w.Printf("class RawScope:\n")
	w.Printf("\tprev: Optional[int]\n")
	w.Printf("\tnext: Optional[int]\n")
	w.Printf("\tbranch: Optional[int]\n")
	w.Printf("\tident: List[int]\n\n")

	w.Printf("class Ast:\n")
	w.Printf("\tnode: List[RawNode]\n")
	w.Printf("\tscope: List[RawScope]\n\n")

	w.Printf("class AstFile:\n")
	w.Printf("\tfiles: List[str]\n")
	w.Printf("\tast: Optional[Ast]\n")
	w.Printf("\terror: Optional[SrcError]\n\n")

	w.Printf("class TokenFile:\n")
	w.Printf("\tfiles: List[str]\n")
	w.Printf("\ttokens: List[Token]\n")
	w.Printf("\terror: Optional[SrcError]\n\n")

}

func main() {
	if len(os.Args) != 2 {
		log.Println("usage: gen_ast2py <file>")
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
		"uint":    "int",
		"uintptr": "int",
		"string":  "str",
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
