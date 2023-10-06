package main

import (
	"encoding/json"
	"io"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"

	"github.com/on-keyday/brgen/src/tool/gen"
)

func generate(w io.Writer, list []gen.Def) {
	writer := gen.NewWriter(w)
	writer.Println(`// Code generated by gen_ast2go; DO NOT EDIT.`)
	writer.Println(``)
	writer.Println(`package ast2go`)
	writer.Println(``)
	writer.Println(`import (`)
	writer.Println(`	"encoding/json"`)
	writer.Println(`	"fmt"`)
	writer.Println(`)`)
	writer.Println(``)

	var scopeDef *gen.Struct

	var nodeStructMap = make(map[string]*gen.Struct)
	var nodeInterfaceMap = make(map[string]*gen.Interface)

	for _, def := range list {
		switch d := def.(type) {
		case *gen.Interface:

			writer.Printf("type %s interface {\n", d.Name)
			writer.Printf("	%s()\n", d.Method)
			if d.Embed != "" {
				writer.Printf("	%s\n", d.Embed)
			}
			writer.Printf("}\n\n")

			nodeInterfaceMap[d.Name] = d

		case *gen.Struct:

			writer.Printf("type %s struct {\n", d.Name)
			isJSON := len(d.Implements) == 0 && d.Name != "Scope"
			for _, field := range d.Fields {
				if isJSON {
					writer.Printf("	%s %s `json:%q`\n", field.Name, field.Type, field.Tag)

				} else {
					writer.Printf("	%s %s\n", field.Name, field.Type)
				}
			}
			writer.Printf("}\n\n")
			for _, iface := range d.Implements {
				writer.Printf("func (n *%s) %s() {}\n\n", d.Name, iface)
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
			for i, value := range d.Values {
				writer.Printf("	%s%s %s = %d\n", d.Name, value.Name, d.Name, i)
			}
			writer.Printf(")\n\n")
			writer.Printf("func (n %s) String() string {\n", d.Name)
			writer.Printf("	switch n {\n")
			for _, value := range d.Values {
				writer.Printf("	case %s%s:\n", d.Name, value.Name)
				writer.Printf("		return \"%s\"\n", value.Str)
			}
			writer.Printf("	default:\n")
			writer.Printf("		return fmt.Sprintf(\"%s(%%d)\", n)\n", d.Name)
			writer.Printf("	}\n")
			writer.Printf("}\n\n")
			writer.Printf("func (n %s) UnmarshalJSON(data []byte) error {\n", d.Name)
			writer.Printf("	var tmp string\n")
			writer.Printf("	if err := json.Unmarshal(data, &tmp); err != nil {\n")
			writer.Printf("		return err\n")
			writer.Printf("	}\n")
			writer.Printf("	switch tmp {\n")
			for _, value := range d.Values {
				writer.Printf("	case %q:\n", value.Str)
				writer.Printf("		n = %s%s\n", d.Name, value.Name)
			}
			writer.Printf("	default:\n")
			writer.Printf("		return fmt.Errorf(\"unknown %s: %%q\", tmp)\n", d.Name)
			writer.Printf("	}\n")
			writer.Printf("	return nil\n")
			writer.Printf("}\n\n")
		}
	}

	writer.Printf("type astConstructor struct {\n")
	writer.Printf("	node []Node\n")
	writer.Printf("	scope []*Scope\n")
	writer.Printf("}\n")

	writer.Printf("type rawNode struct {\n")
	writer.Printf("	NodeType string `json:\"node_type\"`\n")
	writer.Printf("	Loc      Loc    `json:\"loc\"`\n")
	writer.Printf("	Body     json.RawMessage\n")
	writer.Printf("}\n")

	writer.Printf("type rawScope struct {\n")
	for _, field := range scopeDef.Fields {
		writer.Printf("	%s %s `json:%q`\n", field.Name, field.Type.UintptrString(), field.Tag)
	}
	writer.Printf("}\n")

	writer.Printf("type AST struct {\n")
	writer.Printf("	*Program\n")
	writer.Printf("}\n\n")

	writer.Printf("type File struct {\n")
	writer.Printf("	Files []string `json:\"files\"`\n")
	writer.Printf("	Ast *AST `json:\"ast\"`\n")
	writer.Printf("	Error string `json:\"error\"`\n")
	writer.Printf("}\n\n")

	writer.Printf("func (n *AST) UnmarshalJSON(data []byte) error {\n")
	writer.Printf("	var tmp astConstructor\n")
	writer.Printf("	prog, err := tmp.unmarshal(data);")
	writer.Printf("	if  err != nil {\n")
	writer.Printf("		return err\n")
	writer.Printf("	}\n")
	writer.Printf("	n.Program = prog\n")
	writer.Printf("	return nil\n")
	writer.Printf("}\n")

	writer.Printf("func (n *astConstructor) unmarshal(data []byte) (prog *Program, err error) {\n")
	writer.Printf("defer func() {\n")
	writer.Printf("	if r := recover(); r != nil {\n")
	writer.Printf("		err = fmt.Errorf(\"%%v\", r)\n")
	writer.Printf("	}\n")
	writer.Printf("}()\n")
	writer.Printf("	var aux struct {\n")
	writer.Printf("		Node []rawNode `json:\"node\"`\n")
	writer.Printf("		Scope []*rawScope `json:\"scope\"`\n")
	writer.Printf("	}\n")
	writer.Printf("	if err = json.Unmarshal(data, &aux); err != nil {\n")
	writer.Printf("		return nil,err\n")
	writer.Printf("	}\n")
	writer.Printf("	n.node = make([]Node, len(aux.Node))\n")
	writer.Printf("	for _, raw := range aux.Node {\n")
	writer.Printf("		switch raw.NodeType {\n")
	for _, def := range list {
		if structDef, ok := def.(*gen.Struct); ok {
			if len(structDef.Implements) == 0 {
				continue
			}
			writer.Printf("		case %q:\n", structDef.NodeType)
			writer.Printf("			n.node = append(n.node, &%s{Loc: raw.Loc})\n", structDef.Name)
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
	for _, def := range list {
		if structDef, ok := def.(*gen.Struct); ok {
			if len(structDef.Implements) == 0 {
				continue
			}
			writer.Printf("		case %q:\n", structDef.NodeType)
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
					writer.Printf("			v.%s = make([]%s, len(tmp.%s))\n", field.Name, &typ, field.Name)
					writer.Printf("			for j, k := range tmp.%s {\n", field.Name)
					writer.Printf("				v.%s[j] = n.node[k].(%s)\n", field.Name, &typ)
					writer.Printf("			}\n")
				} else if field.Type.Name == "Scope" {
					writer.Printf("			if tmp.%s != nil {\n", field.Name)
					writer.Printf("				v.%s = n.scope[*tmp.%s]\n", field.Name, field.Name)
					writer.Printf("			}\n")
				} else if field.Type.IsPtr || field.Type.IsInterface {
					writer.Printf("			if tmp.%s != nil {\n", field.Name)
					writer.Printf("				v.%s = n.node[*tmp.%s].(%s)\n", field.Name, field.Name, field.Type)
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
			writer.Printf("		n.scope[i].%s = make([]%s, len(raw.%s))\n", field.Name, &typ, field.Name)
			writer.Printf("		for j, k := range raw.%s {\n", field.Name)
			writer.Printf("			n.scope[i].%s[j] = n.node[k].(%s)\n", field.Name, &typ)
			writer.Printf("		}\n")
		}
	}
	writer.Printf("	}\n")

	writer.Printf("	return n.node[0].(*Program), nil\n")
	writer.Printf("}\n\n")

	writer.Printf("func Walk(n Node, f func(Node) (cont bool)) {\n")
	writer.Printf("	if !f(n) {\n")
	writer.Printf("		return\n")
	writer.Printf("	}\n")
	writer.Printf("	switch v := n.(type) {\n")
	for _, def := range list {
		if structDef, ok := def.(*gen.Struct); ok {
			if len(structDef.Implements) == 0 {
				continue
			}
			writer.Printf("	case *%s:\n", structDef.Name)
			for _, field := range structDef.Fields {
				if field.Name == "Loc" {
					continue
				}
				_, node_struct := nodeStructMap[field.Type.Name]
				_, node_interface := nodeInterfaceMap[field.Type.Name]
				if !node_struct && !node_interface {
					continue
				}
				if field.Type.IsArray {
					writer.Printf("		for _, w := range v.%s {\n", field.Name)
					writer.Printf("			Walk(w, f)\n")
					writer.Printf("		}\n")
				} else if field.Type.IsPtr || field.Type.IsInterface {
					writer.Printf("		if v.%s != nil {\n", field.Name)
					writer.Printf("			Walk(v.%s, f)\n", field.Name)
					writer.Printf("		}\n")
				} else {
					writer.Printf("		Walk(v.%s, f)\n", field.Name)
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
	// execute command in executable directory (not current directory)
	//get executable directory
	path, err := os.Executable()
	if err != nil {
		log.Println(err)
		return
	}

	// exec "{path}/src2json --dump-ast" with exec package
	path = filepath.Join(filepath.Dir(path), "src2json")

	if runtime.GOOS == "windows" {
		path += ".exe"
	}

	cmd := exec.Command(path, "--dump-ast", "--dump-enum-name")

	// get stdout
	stdout, err := cmd.StdoutPipe()
	err = cmd.Start()
	if err != nil {
		log.Println(err)
		return
	}
	list := &gen.List{}
	err = json.NewDecoder(stdout).Decode(list)
	if err != nil {
		log.Println(err)
		return
	}

	err = cmd.Wait()
	if err != nil {
		log.Println(err)
		return
	}

	defs, err := gen.CollectDefinition(list)
	if err != nil {
		log.Println(err)
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
