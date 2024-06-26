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
	writer := gen.NewWriter(w)
	writer.Println(`// Code generated by gen_ast2go; DO NOT EDIT.`)
	writer.Println(``)
	writer.Println(`package ast`)
	writer.Println(``)
	writer.Println(`import (`)
	writer.Println(`	"encoding/json"`)
	writer.Println(`	"fmt"`)
	writer.Println(`)`)
	writer.Println(``)

	var scopeDef *gen.Struct

	var nodeStructMap = make(map[string]*gen.Struct)

	for _, def := range list.Defs {
		switch d := def.(type) {
		case *gen.Interface:

			writer.Printf("type %s interface {\n", d.Name)
			writer.Printf("	is%s()\n", d.Name)
			if d.Embed != "" {
				writer.Printf("	%s\n", d.Embed)
			}
			for _, c := range d.UnCommonFields {
				writer.Printf("	Get%s() %s\n", c.Name, c.Type.GoString())
			}
			if d.Name == "Node" {
				writer.Printf("	GetNodeType() NodeType\n")
			}
			writer.Printf("}\n\n")

		case *gen.Struct:

			writer.Printf("type %s struct {\n", d.Name)
			isJSON := len(d.Implements) == 0 && d.Name != "Scope"
			for _, field := range d.Fields {
				if isJSON {
					writer.Printf("	%s %s `json:%q`\n", field.Name, field.Type.GoString(), field.Tag)

				} else {
					writer.Printf("	%s %s\n", field.Name, field.Type.GoString())
				}
			}
			writer.Printf("}\n\n")
			for _, iface := range d.Implements {
				writer.Printf("func (n *%s) is%s() {}\n\n", d.Name, iface)
				if found, ok := list.Interfaces[iface]; ok {
					for _, c := range found.UnCommonFields {
						writer.Printf("func (n *%s) Get%s() %s {\n", d.Name, c.Name, c.Type.GoString())
						writer.Printf("	return n.%s\n", c.Name)
						writer.Printf("}\n\n")
					}
				}
			}

			if d.Name == "Scope" {
				scopeDef = d
			} else {
				if !isJSON {
					nodeStructMap[d.NodeType] = d
				}
			}

		case *gen.Enum:
			writer.Printf("type %s int\n", d.Name)
			writer.Printf("const (\n")
			for _, value := range d.Values {
				writer.Printf("	%s%s %s = %s\n", d.Name, value.Name, d.Name, value.NumericValue)
			}
			writer.Printf(")\n\n")
			writer.Printf("func (n %s) String() string {\n", d.Name)
			if d.IsBitField {
				writer.Printf("	var s string\n")
				zeroCase := ""
				for _, value := range d.Values {
					if value.NumericValue == "0" {
						zeroCase = value.Value
						continue
					}
					writer.Printf("	if n&%s%s!=0 {\n", d.Name, value.Name)
					writer.Printf("		if s!=\"\" {\n")
					writer.Printf("			s+=\" | \"\n")
					writer.Printf("		}\n")
					writer.Printf("		s+=\"%s\"\n", value.Value)
					writer.Printf("	}\n")
				}
				writer.Printf("	if s==\"\" {\n")
				if zeroCase != "" {
					writer.Printf("		return \"%s\"\n", zeroCase)
				} else {
					writer.Printf("		return fmt.Sprintf(\"%s(%%d)\", n)\n", d.Name)
				}
				writer.Printf("	}\n")
				writer.Printf("	return s\n")
			} else {
				writer.Printf("	switch n {\n")
				for _, value := range d.Values {
					writer.Printf("	case %s%s:\n", d.Name, value.Name)
					writer.Printf("		return \"%s\"\n", value.Value)
				}
				writer.Printf("	default:\n")
				writer.Printf("		return fmt.Sprintf(\"%s(%%d)\", n)\n", d.Name)
				writer.Printf("	}\n")
			}
			writer.Printf("}\n\n")
			writer.Printf("func (n *%s) UnmarshalJSON(data []byte) error {\n", d.Name)
			if d.IsBitField {
				writer.Printf("	var tmp int\n")
			} else {
				writer.Printf("	var tmp string\n")
			}
			writer.Printf("	if err := json.Unmarshal(data, &tmp); err != nil {\n")
			writer.Printf("		return err\n")
			writer.Printf("	}\n")
			if d.IsBitField {
				writer.Printf("	*n = %s(tmp)\n", d.Name)
			} else {
				writer.Printf("	switch tmp {\n")
				for _, value := range d.Values {
					writer.Printf("	case %q:\n", value.Value)
					writer.Printf("		*n = %s%s\n", d.Name, value.Name)
				}
				writer.Printf("	default:\n")
				writer.Printf("		return fmt.Errorf(\"unknown %s: %%q\", tmp)\n", d.Name)
				writer.Printf("	}\n")
			}
			writer.Printf("	return nil\n")
			writer.Printf("}\n\n")
			if d.Name == "NodeType" {
				for _, value := range d.Values {
					if _, ok := list.Interfaces[strcase.ToCamel(value.Name)]; ok {
						continue
					}
					writer.Printf("func (n *%s) GetNodeType() NodeType {\n", strcase.ToCamel(value.Name))
					writer.Printf("	return NodeType%s\n", strcase.ToCamel(value.Name))
					writer.Printf("}\n\n")
				}
			}
		}
	}

	writer.Printf("type astConstructor struct {\n")
	writer.Printf("	node []Node\n")
	writer.Printf("	scope []*Scope\n")
	writer.Printf("}\n")

	writer.Printf("func ParseAST(aux *JsonAst) (prog *Program, err error) {\n")
	writer.Printf("defer func() {\n")
	writer.Printf("	if r := recover(); r != nil {\n")
	writer.Printf("		err = fmt.Errorf(\"%%v\", r)\n")
	writer.Printf("	}\n")
	writer.Printf("}()\n")
	writer.Printf("	n := &astConstructor{}\n")
	writer.Printf("	n.node = make([]Node, len(aux.Node))\n")
	writer.Printf("	for i, raw := range aux.Node {\n")
	writer.Printf("		switch raw.NodeType {\n")
	for _, def := range list.Defs {
		if structDef, ok := def.(*gen.Struct); ok {
			if len(structDef.Implements) == 0 {
				continue
			}
			writer.Printf("		case NodeType%s:\n", strcase.ToCamel(structDef.NodeType))
			writer.Printf("			n.node[i] = &%s{Loc: raw.Loc}\n", structDef.Name)
		}
	}
	writer.Printf("		default:\n")
	writer.Printf("			return nil, fmt.Errorf(\"unknown node type: %%q\", raw.NodeType)\n")
	writer.Printf("		}\n")
	writer.Printf("	}\n")
	writer.Printf("	n.scope = make([]*Scope, len(aux.Scope))\n")
	writer.Printf("	for i := range aux.Scope {\n")
	writer.Printf("		n.scope[i] = &Scope{}\n")
	writer.Printf("	}\n")

	writer.Printf("	for i, raw := range aux.Node {\n")
	writer.Printf("		switch raw.NodeType {\n")
	for _, def := range list.Defs {
		if structDef, ok := def.(*gen.Struct); ok {
			if len(structDef.Implements) == 0 {
				continue
			}
			writer.Printf("		case NodeType%s:\n", strcase.ToCamel(structDef.NodeType))
			if len(structDef.Fields) > 1 {
				writer.Printf("			v:=n.node[i].(*%s)\n", structDef.Name)
			}
			writer.Printf("			var tmp struct {\n")
			for _, field := range structDef.Fields {
				if field.Name == "Loc" {
					continue
				}
				writer.Printf("				%s %s `json:%q`\n", field.Name, field.Type.UintptrString(), field.Tag)
			}
			writer.Printf("			}\n")
			writer.Printf("			if err := json.Unmarshal(raw.Body, &tmp); err != nil {\n")
			writer.Printf("				return nil,err\n")
			writer.Printf("			}\n")
			for _, field := range structDef.Fields {
				if field.Name == "Loc" {
					continue
				}

				if field.Type.IsArray {
					typ := *field.Type
					typ.IsArray = false
					writer.Printf("			v.%s = make([]%s, len(tmp.%s))\n", field.Name, typ.GoString(), field.Name)
					writer.Printf("			for j, k := range tmp.%s {\n", field.Name)
					writer.Printf("				v.%s[j] = n.node[k].(%s)\n", field.Name, typ.GoString())
					writer.Printf("			}\n")
				} else if field.Type.Name == "Scope" {
					writer.Printf("			if tmp.%s != nil {\n", field.Name)
					writer.Printf("				v.%s = n.scope[*tmp.%s]\n", field.Name, field.Name)
					writer.Printf("			}\n")
				} else if field.Type.IsPtr || field.Type.IsInterface {
					writer.Printf("			if tmp.%s != nil {\n", field.Name)
					writer.Printf("				v.%s = n.node[*tmp.%s].(%s)\n", field.Name, field.Name, field.Type.GoString())
					writer.Printf("			}\n")
				} else {
					writer.Printf("			v.%s = tmp.%s\n", field.Name, field.Name)
				}
			}
		}
	}
	writer.Printf("		default:\n")
	writer.Printf("			return nil, fmt.Errorf(\"unknown node type: %%q\", raw.NodeType)\n")
	writer.Printf("		}\n")
	writer.Printf("	}\n")
	writer.Printf("	for i, raw := range aux.Scope {\n")
	for _, field := range scopeDef.Fields {
		if field.Type.Name == "Scope" {
			writer.Printf("		if raw.%s != nil {\n", field.Name)
			writer.Printf("			n.scope[i].%s = n.scope[*raw.%s]\n", field.Name, field.Name)
			writer.Printf("		}\n")
		} else if field.Type.IsArray {
			typ := *field.Type
			typ.IsArray = false
			writer.Printf("		n.scope[i].%s = make([]%s, len(raw.%s))\n", field.Name, typ.GoString(), field.Name)
			writer.Printf("		for j, k := range raw.%s {\n", field.Name)
			writer.Printf("			n.scope[i].%s[j] = n.node[k].(%s)\n", field.Name, typ.GoString())
			writer.Printf("		}\n")
		} else if field.Type.IsPtr || field.Type.IsInterface {
			writer.Printf("		if raw.%s != nil {\n", field.Name)
			writer.Printf("			n.scope[i].%s = n.node[*raw.%s].(%s)\n", field.Name, field.Name, field.Type.GoString())
			writer.Printf("		}\n")
		} else {
			writer.Printf("		n.scope[i].%s = raw.%s\n", field.Name, field.Name)
		}
	}
	writer.Printf("	}\n")

	writer.Printf("	return n.node[0].(*Program), nil\n")
	writer.Printf("}\n\n")

	writer.Printf("type Visitor interface {\n")
	writer.Printf("	Visit(v Visitor,n Node) bool\n")
	writer.Printf("}\n\n")

	writer.Printf("type VisitFn func(v Visitor,n Node) bool\n\n")
	writer.Printf("func (f VisitFn) Visit(v Visitor,n Node) bool {\n")
	writer.Printf("	return f(v,n)\n")
	writer.Printf("}\n\n")

	writer.Printf("func Walk(n Node, f Visitor) {\n")
	writer.Printf("	switch v := n.(type) {\n")
	for _, def := range list.Defs {
		if structDef, ok := def.(*gen.Struct); ok {
			if len(structDef.Implements) == 0 {
				continue
			}
			writer.Printf("	case *%s:\n", structDef.Name)
			for _, field := range structDef.Fields {
				if field.Type.Name == "Scope" {
					continue
				}
				if field.Type.IsWeak {
					continue // avoid infinite loop
				}
				if field.Type.IsArray {
					writer.Printf("		for _, w := range v.%s {\n", field.Name)
					writer.Printf("			if !f.Visit(f,w) {\n")
					writer.Printf("				return\n")
					writer.Printf("			}\n")
					writer.Printf("		}\n")
				} else if field.Type.IsPtr || field.Type.IsInterface {
					writer.Printf("		if v.%s != nil {\n", field.Name)
					writer.Printf("			if !f.Visit(f,v.%s) {\n", field.Name)
					writer.Printf("				return\n")
					writer.Printf("			}\n")
					writer.Printf("		}\n")
				}
			}
		}
	}
	writer.Printf("	}\n")
	writer.Printf("}\n\n")

}

func main() {
	if len(os.Args) != 2 {
		log.Println("usage: gen_ast2go <file>")
		return
	}

	file := os.Args[1]

	log.SetFlags(log.Flags() | log.Lshortfile)

	list, err := gen.LoadFromDefaultSrc2JSON()

	if err != nil {
		log.Println(err)
		return
	}

	defs, err := gen.CollectDefinition(list, strcase.ToCamel, strcase.ToCamel, map[string]string{
		"uint": "uint64",
		"any":  "json.RawMessage",
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
