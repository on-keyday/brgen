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
	w.Printf("from typing import Optional,List,Dict,Any,Callable\n\n")
	w.Printf("from enum import Enum as PyEnum\n\n")

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
				w.Printf("    %s: %s\n", field.Name, field.Type.PyString())
				count++
			}
			if count == 0 {
				w.Printf("    pass\n")
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
				w.Printf("    %s: %s\n", field.Name, field.Type.PyString())
				count++
			}
			if count == 0 {
				w.Printf("    pass\n")
			}

			if len(v.Implements) == 0 && v.Name != "Scope" {
				w.Printf("\n")
				w.Printf("def parse_%s(json: dict) -> %s:\n", v.Name, v.Name)
				w.Printf("    ret = %s()\n", v.Name)
				isBuiltinType := func(s string) bool {
					return s == "bool" || s == "int" || s == "str"
				}
				for _, field := range v.Fields {
					if field.Type.Name == "Any" {
						w.Printf("    ret.%s = json[%q]\n", field.Name, field.Name)
					} else if field.Type.IsArray {
						s := "x"
						if isBuiltinType(field.Type.Name) {
							s = field.Type.Name + "(x)"
						} else {
							s = "parse_" + field.Type.Name + "(x)"
						}
						if field.Type.IsOptional {
							w.Printf("    if json[%q] is not None:\n", field.Name)
							w.Printf("        ret.%s = [%s for x in json[%q]]\n", field.Name, s, field.Name)
							w.Printf("    else:\n")
							w.Printf("        ret.%s = None\n", field.Name)
						} else {
							w.Printf("    ret.%s = [%s for x in json[%q]]\n", field.Name, s, field.Name)
						}
					} else if isBuiltinType(field.Type.Name) {
						if field.Type.IsOptional {
							w.Printf("    if json[%q] is not None:\n", field.Name)
							w.Printf("        ret.%s = %s(json[%q])\n", field.Name, field.Type.Name, field.Name)
							w.Printf("    else:\n")
							w.Printf("        ret.%s = None\n", field.Name)
						} else {
							w.Printf("    ret.%s = %s(json[%q])\n", field.Name, field.Type.Name, field.Name)
						}
					} else {
						_, found := defs.Enums[field.Type.Name]
						if found {
							w.Printf("    ret.%s = %s(json[%q])\n", field.Name, field.Type.PyString(), field.Name)
						} else {
							if field.Type.IsOptional {
								w.Printf("    if json[%q] is not None:\n", field.Name)
								w.Printf("        ret.%s = parse_%s(json[%q])\n", field.Name, field.Type.Name, field.Name)
								w.Printf("    else:\n")
								w.Printf("        ret.%s = None\n", field.Name)
							} else {
								w.Printf("    ret.%s = parse_%s(json[%q])\n", field.Name, field.Type.Name, field.Name)
							}
						}
					}
				}
				w.Printf("    return ret\n\n")
			}

		case *gen.Enum:
			w.Printf("class %s(PyEnum):\n", v.Name)
			for _, field := range v.Values {
				w.Printf("    %s = %q\n", strings.ToUpper(field.Name), field.Value)
			}
		}
		w.Printf("\n\n")
	}

	w.Printf("def raiseError(err):\n")
	w.Printf("    raise err\n\n")

	w.Printf("def ast2node(ast :JsonAst) -> Program:\n")
	w.Printf("    if not isinstance(ast,JsonAst):\n")
	w.Printf("        raise TypeError('ast must be JsonAst')\n")
	w.Printf("    node = list()\n")
	w.Printf("    for raw in ast.node:\n")
	w.Printf("        match raw.node_type:\n")
	for _, def := range defs.Defs {
		switch v := def.(type) {
		case *gen.Struct:
			if len(v.Implements) == 0 {
				continue
			}
			w.Printf("            case NodeType.%s:\n", strings.ToUpper(strcase.ToSnake(v.Name)))
			w.Printf("                node.append(%s())\n", v.Name)
		}
	}
	w.Printf("            case _:\n")
	w.Printf("                raise TypeError('unknown node type')\n")
	w.Printf("    scope = [Scope() for _ in range(len(ast.scope))]\n")
	w.Printf("    for i in range(len(ast.node)):\n")
	w.Printf("        node[i].loc = ast.node[i].loc\n")
	w.Printf("        match ast.node[i].node_type:\n")
	for _, def := range defs.Defs {
		switch v := def.(type) {
		case *gen.Struct:
			if len(v.Implements) == 0 {
				continue
			}
			w.Printf("            case NodeType.%s:\n", strings.ToUpper(strcase.ToSnake(v.Name)))
			count := 0
			for _, field := range v.Fields {
				if field.Name == "loc" {
					continue
				}
				if field.Type.Name == "Scope" {
					w.Printf("                if ast.node[i].body[%q] is not None:\n", field.Name)
					w.Printf("                    node[i].%s = scope[ast.node[i].body[%q]]\n", field.Name, field.Name)
					w.Printf("                else:\n")
					w.Printf("                    node[i].%s = None\n", field.Name)
				} else if field.Type.Name == "Loc" {
					w.Printf("                node[i].%s = parse_Loc(ast.node[i].body[%q])\n", field.Name, field.Name)
				} else if field.Type.Name == "bool" || field.Type.Name == "int" || field.Type.Name == "str" {
					w.Printf("                x = ast.node[i].body[%q]\n", field.Name)
					if field.Type.IsOptional {
						w.Printf("                if x is not None:\n")
						w.Printf("                    node[i].%s = x if isinstance(x,%s) else raiseError(TypeError('type mismatch at %s::%s'))\n", field.Name, field.Type.Name, v.Name, field.Name)
						w.Printf("                else:\n")
						w.Printf("                    node[i].%s = None\n", field.Name)
					} else {
						w.Printf("                node[i].%s = x if isinstance(x,%s)  else raiseError(TypeError('type mismatch at %s::%s'))\n", field.Name, field.Type.Name, v.Name, field.Name)
					}
				} else if field.Type.IsArray {
					w.Printf("                node[i].%s = [(node[x] if isinstance(node[x],%s) else raiseError(TypeError('type mismatch at %s::%s'))) for x in ast.node[i].body[%q]]\n", field.Name, field.Type.Name, v.Name, field.Name, field.Name)
				} else if field.Type.IsPtr || field.Type.IsInterface {
					w.Printf("                if ast.node[i].body[%q] is not None:\n", field.Name)
					w.Printf("                    x = node[ast.node[i].body[%q]]\n", field.Name)
					w.Printf("                    node[i].%s = x if isinstance(x,%s) else raiseError(TypeError('type mismatch at %s::%s'))\n", field.Name, field.Type.Name, v.Name, field.Name)
					w.Printf("                else:\n")
					w.Printf("                    node[i].%s = None\n", field.Name)
				} else {
					_, found := defs.Enums[field.Type.Name]
					if found {
						w.Printf("                node[i].%s = %s(ast.node[i].body[%q])\n", field.Name, field.Type.PyString(), field.Name)
					}
				}
				count++
			}
			if count == 0 {
				w.Printf("                pass\n")
			}
		}
	}
	w.Printf("            case _:\n")
	w.Printf("                raise TypeError('unknown node type')\n")
	w.Printf("    for i in range(len(ast.scope)):\n")
	for _, def := range defs.ScopeDef.Fields {
		if def.Type.Name == "Scope" {
			w.Printf("        if ast.scope[i].%s is not None:\n", def.Name)
			w.Printf("            scope[i].%s = scope[ast.scope[i].%s]\n", def.Name, def.Name)
		} else if def.Type.IsArray {
			w.Printf("        scope[i].%s = [(node[x] if isinstance(node[x],%s) else raiseError(TypeError('type mismatch at Scope::%s'))) for x in ast.scope[i].%s]\n", def.Name, def.Type.Name, def.Name, def.Name)
		} else if def.Type.IsPtr || def.Type.IsInterface {
			w.Printf("        if ast.scope[i].%s is not None:\n", def.Name)
			w.Printf("            scope[i].%s = ast.node[ast.scope[i].%s]\n", def.Name, def.Name)
		} else {
			_, found := defs.Enums[def.Type.Name]
			if found {
				w.Printf("        scope[i].%s = %s(ast.scope[i].%s)\n", def.Name, def.Type.PyString(), def.Name)
			} else {
				w.Printf("        scope[i].%s = ast.scope[i].%s\n", def.Name, def.Name)
			}
		}
	}

	w.Printf("    return node[0]\n\n")

	w.Printf("def walk(node: Node, f: Callable[[Callable,Node],None]) -> None:\n")
	w.Printf("    match node:\n")
	for _, def := range defs.Defs {
		switch d := def.(type) {
		case *gen.Struct:
			if len(d.Implements) == 0 {
				continue
			}
			w.Printf("        case x if isinstance(x,%s):\n", d.Name)
			count := 0
			for _, field := range d.Fields {
				if field.Type.Name == "Scope" {
					continue
				}
				if field.Type.IsWeak {
					continue // avoid infinite loop
				}
				if field.Type.IsArray {
					w.Printf("          for i in range(len(x.%s)):\n", field.Name)
					w.Printf("              if f(f,x.%s[i]) == False:\n", field.Name)
					w.Printf("                  return\n")
				} else if field.Type.IsPtr || field.Type.IsInterface {
					w.Printf("          if x.%s is not None:\n", field.Name)
					w.Printf("              if f(f,x.%s) == False:\n", field.Name)
					w.Printf("                  return\n")
				} else {
					continue
				}
				count++
			}
			if count == 0 {
				w.Printf("            pass\n")
			}
		}
	}
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
		"any":     "Any",
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
