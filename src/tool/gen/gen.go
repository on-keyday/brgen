package gen

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"regexp"

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
	Embed  string
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

func CollectDefinition(list *List) (defs []Def, err error) {
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
			if len(node.BaseNodeType) > 0 {
				iface.Embed = strcase.ToCamel(node.BaseNodeType[0])
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

type Writer struct {
	w io.Writer
}

func (w *Writer) Println(s string) {
	io.WriteString(w.w, s+"\n")
}

func (w *Writer) Printf(format string, args ...interface{}) {
	io.WriteString(w.w, fmt.Sprintf(format, args...))
}

func NewWriter(w io.Writer) *Writer {
	return &Writer{w: w}
}
