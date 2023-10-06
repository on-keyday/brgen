package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"runtime"

	"github.com/iancoleman/strcase"
)

type OrderedKey[T any] struct {
	Key   string
	Value T
}

type OrderedKeyList[T any] []OrderedKey[T]

// preserve json order
func (p *OrderedKeyList[T]) UnmarshalJSON(data []byte) error {
	dec := json.NewDecoder(bytes.NewReader(data))
	tok, err := dec.Token()
	if err != nil {
		return err
	}
	if tok != json.Delim('{') {
		return fmt.Errorf("expected '{', got %v", tok)
	}
	for dec.More() {
		tok, err := dec.Token()
		if err != nil {
			return err
		}
		key, ok := tok.(string)
		if !ok {
			return fmt.Errorf("expected string key, got %v", tok)
		}
		var value T
		err = dec.Decode(&value)
		if err != nil {
			return err
		}
		*p = append(*p, OrderedKey[T]{Key: key, Value: value})
	}
	tok, err = dec.Token()
	if err != nil {
		return err
	}
	if tok != json.Delim('}') {
		return fmt.Errorf("expected '}', got %v", tok)
	}
	return nil
}

type Node struct {
	NodeType     string                 `json:"node_type"`
	BaseNodeType []string               `json:"base_node_type"`
	OneOf        []string               `json:"one_of"`
	Body         OrderedKeyList[string] `json:"body"`
}

type Op struct {
	Op   string `json:"op"`
	Name string `json:"name"`
}

type List struct {
	Node       []Node                 `json:"node"`
	UnaryOp    []Op                   `json:"unary_op"`
	BinaryOp   []Op                   `json:"binary_op"`
	IdentUsage []string               `json:"ident_usage"`
	Endian     []string               `json:"endian"`
	Scope      OrderedKeyList[string] `json:"scope"`
	Loc        OrderedKeyList[string] `json:"loc"`
	Pos        OrderedKeyList[string] `json:"pos"`
}

type Def interface {
	def()
}

type Interface struct {
	Name   string
	Method string
}

func (d *Interface) def() {}

type Type struct {
	Name        string
	IsInterface bool
	IsPtr       bool
	IsArray     bool
}

func (d *Type) String() string {
	prefix := ""
	if d.IsArray {
		prefix += "[]"
	}
	if d.IsPtr {
		prefix += "*"
	}
	return prefix + d.Name
}

func (d *Type) UintptrString() string {
	prefix := ""
	if d.IsArray {
		prefix += "[]"
	}
	if d.IsPtr || d.IsInterface {
		if !d.IsArray {
			prefix += "*"
		}
		return prefix + "uintptr"
	}
	return prefix + d.Name
}

type Field struct {
	Name string
	Type *Type
	Tag  string
}

type Struct struct {
	NodeType   string
	Name       string
	Implements []string
	Fields     []*Field
}

type EnumValue struct {
	Name string
	Str  string
}

type Enum struct {
	Name   string
	Values []*EnumValue
}

func (d *Struct) def() {}
func (d *Enum) def()   {}

var (
	sharedPtr = regexp.MustCompile("shared_ptr<(.*)>")
	weakPtr   = regexp.MustCompile("weak_ptr<(.*)>")
	array     = regexp.MustCompile("array<(.*)>")
)

func convertType(typ string, interfacesMap map[string]*Interface) *Type {
	arrayMatch := array.FindStringSubmatch(typ)
	isPtr := false
	isArray := false
	isInterface := false
	if len(arrayMatch) > 0 {
		typ = arrayMatch[1]
		isArray = true
	}
	sharedPtrMatch := sharedPtr.FindStringSubmatch(typ)
	if len(sharedPtrMatch) > 0 {
		typ = sharedPtrMatch[1]
		if _, ok := interfacesMap[typ]; ok {
			isInterface = true
		} else {
			isPtr = true
		}
	}
	weakPtrMatch := weakPtr.FindStringSubmatch(typ)
	if len(weakPtrMatch) > 0 {
		typ = weakPtrMatch[1]
		if _, ok := interfacesMap[typ]; ok {
			isInterface = true
		} else {
			isPtr = true
		}
	}
	if typ != "string" && typ != "uint" && typ != "bool" && typ != "uintptr" {
		typ = strcase.ToCamel(typ)
	}
	if typ == "uint" {
		typ = "uint64"
	}
	return &Type{
		Name:        typ,
		IsInterface: isInterface,
		IsPtr:       isPtr,
		IsArray:     isArray,
	}
}

func mapToStructFields(m OrderedKeyList[string], interfacesMap map[string]*Interface) (fields []*Field) {
	for _, kv := range m {
		name := kv.Key
		typ := kv.Value
		field := Field{}
		field.Tag = name
		field.Name = strcase.ToCamel(name)
		field.Type = convertType(typ, interfacesMap)
		fields = append(fields, &field)
	}
	return
}

func mapOpsToEnumValues(ops []Op) (values []*EnumValue) {
	for _, op := range ops {
		value := EnumValue{}
		value.Name = strcase.ToCamel(op.Name)
		value.Str = op.Op
		values = append(values, &value)
	}
	return
}

func mapStringsToEnumValues(strings []string) (values []*EnumValue) {
	for _, str := range strings {
		value := EnumValue{}
		value.Name = strcase.ToCamel(str)
		value.Str = str
		values = append(values, &value)
	}
	return
}

func collectDefinition(list *List) (defs []Def, err error) {
	defs = make([]Def, 0)
	var interfacesMap map[string]*Interface

	interfacesMap = make(map[string]*Interface)

	// collect interface
	for _, node := range list.Node {
		if len(node.OneOf) > 0 {
			name := strcase.ToCamel(node.NodeType)
			iface := &Interface{
				Name:   name,
				Method: "is" + name,
			}
			defs = append(defs, iface)
			interfacesMap[node.NodeType] = iface
		}
	}

	// collect struct
	for _, node := range list.Node {
		if len(node.OneOf) > 0 {
			continue
		}
		var body Struct
		body.NodeType = node.NodeType
		body.Name = strcase.ToCamel(node.NodeType)
		if len(node.BaseNodeType) > 0 {
			body.Implements = make([]string, len(node.BaseNodeType))
			for i, base := range node.BaseNodeType {
				body.Implements[i] = "is" + strcase.ToCamel(base)
			}
		}

		body.Fields = make([]*Field, 0, len(node.Body)+1)

		body.Fields = mapToStructFields(node.Body, interfacesMap)

		// add loc field
		loc := Field{}
		loc.Name = "Loc"
		loc.Type = &Type{Name: "Loc"}
		loc.Tag = "`json:\"loc\"`"
		body.Fields = append(body.Fields, &loc)

		defs = append(defs, &body)
	}

	//generate enum mapping of unary_op, binary_op, ident_usage, endian
	enum := &Enum{}
	enum.Name = "UnaryOp"
	enum.Values = mapOpsToEnumValues(list.UnaryOp)
	defs = append(defs, enum)

	enum = &Enum{}
	enum.Name = "BinaryOp"
	enum.Values = mapOpsToEnumValues(list.BinaryOp)
	defs = append(defs, enum)

	enum = &Enum{}
	enum.Name = "IdentUsage"
	enum.Values = mapStringsToEnumValues(list.IdentUsage)
	defs = append(defs, enum)

	enum = &Enum{}
	enum.Name = "Endian"
	enum.Values = mapStringsToEnumValues(list.Endian)
	defs = append(defs, enum)

	// scope definition
	scope := Struct{}
	scope.Name = "Scope"
	scope.Fields = mapToStructFields(list.Scope, interfacesMap)
	defs = append(defs, &scope)

	// loc definition
	loc := Struct{}
	loc.Name = "Loc"
	loc.Fields = make([]*Field, 0, len(list.Loc))
	loc.Fields = mapToStructFields(list.Loc, interfacesMap)
	defs = append(defs, &loc)

	// pos definition
	pos := Struct{}
	pos.Name = "Pos"
	pos.Fields = make([]*Field, 0, len(list.Pos))
	pos.Fields = mapToStructFields(list.Pos, interfacesMap)
	defs = append(defs, &pos)

	return
}

type writer struct {
	w io.Writer
}

func (w *writer) println(s string) {
	io.WriteString(w.w, s+"\n")
}

func (w *writer) printf(format string, args ...interface{}) {
	io.WriteString(w.w, fmt.Sprintf(format, args...))
}

func generate(w io.Writer, list []Def) {
	writer := &writer{w}
	writer.println(`// Code generated by src2json; DO NOT EDIT.`)
	writer.println(``)
	writer.println(`package ast`)
	writer.println(``)
	writer.println(`import (`)
	writer.println(`	"encoding/json"`)
	writer.println(`	"fmt"`)
	writer.println(`)`)
	writer.println(``)

	var scopeDef *Struct

	for _, def := range list {
		switch d := def.(type) {
		case *Interface:

			writer.printf("type %s interface {\n", d.Name)
			writer.printf("	%s()\n", d.Method)
			writer.printf("}\n\n")

		case *Struct:

			writer.printf("type %s struct {\n", d.Name)
			isJSON := len(d.Implements) == 0 && d.Name != "Scope"
			for _, field := range d.Fields {
				if isJSON {
					writer.printf("	%s %s `json:%q`\n", field.Name, field.Type, field.Tag)

				} else {
					writer.printf("	%s %s\n", field.Name, field.Type)
				}
			}
			writer.printf("}\n\n")
			for _, iface := range d.Implements {
				writer.printf("func (n *%s) %s() {}\n\n", d.Name, iface)
			}

			if d.Name == "Scope" {

				scopeDef = d
			}

		case *Enum:
			writer.printf("type %s int\n", d.Name)
			writer.printf("const (\n")
			for i, value := range d.Values {
				writer.printf("	%s%s %s = %d\n", d.Name, value.Name, d.Name, i)
			}
			writer.printf(")\n\n")
			writer.printf("func (n %s) String() string {\n", d.Name)
			writer.printf("	switch n {\n")
			for _, value := range d.Values {
				writer.printf("	case %s%s:\n", d.Name, value.Name)
				writer.printf("		return \"%s\"\n", value.Str)
			}
			writer.printf("	default:\n")
			writer.printf("		return fmt.Sprintf(\"%s(%%d)\", n)\n", d.Name)
			writer.printf("	}\n")
			writer.printf("}\n\n")
			writer.printf("func (n %s) UnmarshalJSON(data []byte) error {\n", d.Name)
			writer.printf("	var tmp string\n")
			writer.printf("	if err := json.Unmarshal(data, &tmp); err != nil {\n")
			writer.printf("		return err\n")
			writer.printf("	}\n")
			writer.printf("	switch tmp {\n")
			for _, value := range d.Values {
				writer.printf("	case %q:\n", value.Str)
				writer.printf("		n = %s%s\n", d.Name, value.Name)
			}
			writer.printf("	default:\n")
			writer.printf("		return fmt.Errorf(\"unknown %s: %%q\", tmp)\n", d.Name)
			writer.printf("	}\n")
			writer.printf("	return nil\n")
			writer.printf("}\n\n")
		}
	}

	writer.printf("type AST struct {\n")
	writer.printf("	node []Node\n")
	writer.printf("	scope []*Scope\n")
	writer.printf("}\n")

	writer.printf("type rawNode struct {\n")
	writer.printf("	NodeType string `json:\"node_type\"`\n")
	writer.printf("	Loc      Loc    `json:\"loc\"`\n")
	writer.printf("	Body     json.RawMessage\n")
	writer.printf("}\n")

	writer.printf("type rawScope struct {\n")
	for _, field := range scopeDef.Fields {
		writer.printf("	%s %s `json:%q`\n", field.Name, field.Type.UintptrString(), field.Tag)
	}
	writer.printf("}\n")

	writer.printf("func (n *AST) UnmarshalJSON(data []byte) error {\n")
	writer.printf("	var aux struct {\n")
	writer.printf("		Node []rawNode `json:\"node\"`\n")
	writer.printf("		Scope []*rawScope `json:\"scope\"`\n")
	writer.printf("	}\n")
	writer.printf("	if err := json.Unmarshal(data, &aux); err != nil {\n")
	writer.printf("		return err\n")
	writer.printf("	}\n")
	writer.printf("	n.node = make([]Node, len(aux.Node))\n")
	writer.printf("	for _, raw := range aux.Node {\n")
	writer.printf("		switch raw.NodeType {\n")
	for _, def := range list {
		if structDef, ok := def.(*Struct); ok {
			if len(structDef.Implements) == 0 {
				continue
			}
			writer.printf("		case %q:\n", structDef.NodeType)
			writer.printf("			n.node = append(n.node, &%s{Loc: raw.Loc})\n", structDef.Name)
		}
	}
	writer.printf("		default:\n")
	writer.printf("			return fmt.Errorf(\"unknown node type: %%q\", raw.NodeType)\n")
	writer.printf("		}\n")
	writer.printf("	}\n")
	writer.printf("	n.scope = make([]*Scope, len(aux.Scope))\n")
	writer.printf("	for i := range aux.Scope {\n")
	writer.printf("		n.scope[i] = &Scope{}\n")
	writer.printf("	}\n")

	writer.printf("	for i, raw := range aux.Node {\n")
	writer.printf("		switch raw.NodeType {\n")
	for _, def := range list {
		if structDef, ok := def.(*Struct); ok {
			if len(structDef.Implements) == 0 {
				continue
			}
			writer.printf("		case %q:\n", structDef.NodeType)
			if len(structDef.Fields) > 1 {
				writer.printf("			v:=n.node[i].(*%s)\n", structDef.Name)
			}
			writer.printf("			var tmp struct {\n")
			for _, field := range structDef.Fields {
				if field.Name == "Loc" {
					continue
				}
				writer.printf("				%s %s `json:%q`\n", field.Name, field.Type.UintptrString(), field.Tag)
			}
			writer.printf("			}\n")
			writer.printf("			if err := json.Unmarshal(raw.Body, &tmp); err != nil {\n")
			writer.printf("				return err\n")
			writer.printf("			}\n")
			for _, field := range structDef.Fields {
				if field.Name == "Loc" {
					continue
				}

				if field.Type.IsArray {
					typ := *field.Type
					typ.IsArray = false
					writer.printf("			v.%s = make([]%s, len(tmp.%s))\n", field.Name, &typ, field.Name)
					writer.printf("			for j, k := range tmp.%s {\n", field.Name)
					writer.printf("				v.%s[j] = n.node[k].(%s)\n", field.Name, &typ)
					writer.printf("			}\n")
				} else if field.Type.Name == "Scope" {
					writer.printf("			if tmp.%s != nil {\n", field.Name)
					writer.printf("				v.%s = n.scope[*tmp.%s]\n", field.Name, field.Name)
					writer.printf("			}\n")
				} else if field.Type.IsPtr || field.Type.IsInterface {
					writer.printf("			if tmp.%s != nil {\n", field.Name)
					writer.printf("				v.%s = n.node[*tmp.%s].(%s)\n", field.Name, field.Name, field.Type)
					writer.printf("			}\n")
				} else {
					writer.printf("			v.%s = tmp.%s\n", field.Name, field.Name)
				}
			}
		}
	}
	writer.printf("		default:\n")
	writer.printf("			return fmt.Errorf(\"unknown node type: %%q\", raw.NodeType)\n")
	writer.printf("		}\n")
	writer.printf("	}\n")
	writer.printf("	for i, raw := range aux.Scope {\n")
	for _, field := range scopeDef.Fields {
		if field.Type.Name == "Scope" {
			writer.printf("		if raw.%s != nil {\n", field.Name)
			writer.printf("			n.scope[i].%s = n.scope[*raw.%s]\n", field.Name, field.Name)
			writer.printf("		}\n")
		} else if field.Type.IsArray {
			typ := *field.Type
			typ.IsArray = false
			writer.printf("		n.scope[i].%s = make([]%s, len(raw.%s))\n", field.Name, &typ, field.Name)
			writer.printf("		for j, k := range raw.%s {\n", field.Name)
			writer.printf("			n.scope[i].%s[j] = n.node[k].(%s)\n", field.Name, &typ)
			writer.printf("		}\n")
		}
	}
	writer.printf("	}\n")

	writer.printf("	return nil\n")
	writer.printf("}\n")
}

func main() {
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
	list := &List{}
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

	defs, err := collectDefinition(list)
	if err != nil {
		log.Println(err)
		return
	}

	generate(os.Stdout, defs)
}
