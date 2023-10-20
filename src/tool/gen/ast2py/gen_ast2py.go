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
	w.Printf("from enum import Enum\n\n")

	w.Printf("class NodeType(Enum):\n")
	for _, def := range defs.NodeTypes {
		w.Printf("    %s = %q\n", strings.ToUpper(def), def)
	}
	w.Printf("\n\n")

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
				for _, field := range v.Fields {
					if field.Type.Name == "bool" || field.Type.Name == "int" || field.Type.Name == "str" {
						w.Printf("    ret.%s = %s(json[%q])\n", field.Name, field.Type.PyString(), field.Name)
					} else if field.Type.IsArray {
						w.Printf("    ret.%s = [parse_%s(x) for x in json[%q]]\n", field.Name, field.Type.Name, field.Name)
					} else {
						_, found := defs.Enums[field.Type.Name]
						if found {
							w.Printf("    ret.%s = %s(json[%q])\n", field.Name, field.Type.PyString(), field.Name)
						} else {
							w.Printf("    ret.%s = parse_%s(json[%q])\n", field.Name, field.Type.Name, field.Name)
						}
					}
				}
				w.Printf("    return ret\n\n")
			}

		case *gen.Enum:
			w.Printf("class %s(Enum):\n", v.Name)
			for _, field := range v.Values {
				w.Printf("    %s = %q\n", strings.ToUpper(field.Name), field.Str)
			}
		}
		w.Printf("\n\n")
	}

	w.Printf("class RawNode:\n")
	w.Printf("    node_type: NodeType\n")
	w.Printf("    loc: Loc\n")
	w.Printf("    body: Dict[str,Any]\n\n")

	w.Printf("def parse_RawNode(json: dict) -> RawNode:\n")
	w.Printf("    ret = RawNode()\n")
	w.Printf("    ret.node_type = NodeType(json['node_type'])\n")
	w.Printf("    ret.loc = parse_Loc(json['loc'])\n")
	w.Printf("    ret.body = json['body']\n")
	w.Printf("    return ret\n\n")

	w.Printf("class RawScope:\n")
	w.Printf("    prev: Optional[int]\n")
	w.Printf("    next: Optional[int]\n")
	w.Printf("    branch: Optional[int]\n")
	w.Printf("    ident: List[int]\n\n")

	w.Printf("def parse_RawScope(json: dict) -> RawScope:\n")
	w.Printf("    ret = RawScope()\n")
	w.Printf("    ret.prev = json['prev']\n")
	w.Printf("    ret.next = json['next']\n")
	w.Printf("    ret.branch = json['branch']\n")
	w.Printf("    ret.ident = json['ident']\n")
	w.Printf("    return ret\n\n")

	w.Printf("class Ast:\n")
	w.Printf("    node: List[RawNode]\n")
	w.Printf("    scope: List[RawScope]\n\n")

	w.Printf("def parse_Ast(json: dict) -> Ast:\n")
	w.Printf("    ret = Ast()\n")
	w.Printf("    ret.node = [parse_RawNode(x) for x in json['node']]\n")
	w.Printf("    ret.scope = [parse_RawScope(x) for x in json['scope']]\n")
	w.Printf("    return ret\n\n")

	w.Printf("class AstFile:\n")
	w.Printf("    files: List[str]\n")
	w.Printf("    ast: Optional[Ast]\n")
	w.Printf("    error: Optional[SrcError]\n\n")

	w.Printf("class TokenFile:\n")
	w.Printf("    files: List[str]\n")
	w.Printf("    tokens: List[Token]\n")
	w.Printf("    error: Optional[SrcError]\n\n")

	w.Printf("def parse_AstFile(data: dict) -> AstFile:\n")
	w.Printf("    files = data['files']\n")
	w.Printf("    error = data['error']\n")
	w.Printf("    ast = data['ast']\n")
	w.Printf("    if ast is not None:\n")
	w.Printf("        ast = parse_Ast(ast)\n")
	w.Printf("    if error is not None:\n")
	w.Printf("        error = parse_SrcError(error)\n")
	w.Printf("    ret = AstFile()\n")
	w.Printf("    ret.files = files\n")
	w.Printf("    ret.ast = ast\n")
	w.Printf("    ret.error = error\n")
	w.Printf("    return ret\n\n")

	w.Printf("def parse_TokenFile(data: dict) -> TokenFile:\n")
	w.Printf("    files = data['files']\n")
	w.Printf("    error = data['error']\n")
	w.Printf("    tokens = data['tokens']\n")
	w.Printf("    if error is not None:\n")
	w.Printf("        error = parse_SrcError(error)\n")
	w.Printf("    ret = TokenFile()\n")
	w.Printf("    ret.files = files\n")
	w.Printf("    ret.tokens = [parse_Token(x) for x in tokens]\n")
	w.Printf("    ret.error = error\n")
	w.Printf("    return ret\n\n")

	w.Printf("def raiseError(err):\n")
	w.Printf("    raise err\n\n")

	w.Printf("def ast2node(ast :Ast) -> Program:\n")
	w.Printf("    if not isinstance(ast,Ast):\n")
	w.Printf("        raise TypeError('ast must be Ast')\n")
	w.Printf("    node = List[Node]()\n")
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
					w.Printf("                node[i].%s = scope[ast.node[i].body[%q]]\n", field.Name, field.Name)
				} else if field.Type.Name == "Loc" {
					w.Printf("                node[i].%s = parse_Loc(ast.node[i].body[%q])\n", field.Name, field.Name)
				} else if field.Type.Name == "bool" || field.Type.Name == "int" || field.Type.Name == "str" {
					w.Printf("                x = ast.node[i].body[%q]\n", field.Name)
					w.Printf("                node[i].%s = x if isinstance(x,%s)  else raiseError(TypeError('type mismatch'))\n", field.Name, field.Type.Name)
				} else if field.Type.IsArray {
					w.Printf("                node[i].%s = [(node[x] if isinstance(node[x],%s) else raiseError(TypeError('type mismatch'))) for x in ast.node[i].body[%q]]\n", field.Name, field.Type.Name, field.Name)
				} else if field.Type.IsPtr || field.Type.IsInterface {
					w.Printf("                x = node[ast.node[i].body[%q]]\n", field.Name)
					w.Printf("                node[i].%s = x if isinstance(x,%s) or x is None else raiseError(TypeError('type mismatch'))\n", field.Name, field.Type.Name)
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
	w.Printf("        if ast.scope[i].next is not None:\n")
	w.Printf("            scope[i].next = scope[ast.scope[i].next]\n")
	w.Printf("        if ast.scope[i].branch is not None:\n")
	w.Printf("            scope[i].branch = scope[ast.scope[i].branch]\n")
	w.Printf("        if ast.scope[i].prev is not None:\n")
	w.Printf("            scope[i].prev = scope[ast.scope[i].prev]\n")
	w.Printf("        scope[i].ident = [node[x] for x in ast.scope[i].ident]\n")
	w.Printf("    return Program(node[0])\n\n")

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
					w.Printf("              f(f,x.%s[i])\n", field.Name)
				} else if field.Type.IsPtr || field.Type.IsInterface {
					w.Printf("          if x.%s is not None:\n", field.Name)
					w.Printf("              f(f,x.%s)\n", field.Name)
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
