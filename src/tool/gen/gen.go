package gen

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"runtime"
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
	Embed  string
	Fields []*Field // common fields
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

type collector struct {
	fieldCaseFn   func(string) string
	typeCaseFn    func(string) string
	interfacesMap map[string]*Interface
}

func (c *collector) convertType(typ string) *Type {
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
		if _, ok := c.interfacesMap[typ]; ok {
			isInterface = true
		} else {
			isPtr = true
		}
	}
	weakPtrMatch := weakPtr.FindStringSubmatch(typ)
	if len(weakPtrMatch) > 0 {
		typ = weakPtrMatch[1]
		if _, ok := c.interfacesMap[typ]; ok {
			isInterface = true
		} else {
			isPtr = true
		}
	}
	if typ != "string" && typ != "uint" && typ != "bool" && typ != "uintptr" {
		typ = c.typeCaseFn(typ)
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

func (c *collector) mapToStructFields(m OrderedKeyList[string]) (fields []*Field) {
	for _, kv := range m {
		name := kv.Key
		typ := kv.Value
		field := Field{}
		field.Tag = name
		field.Name = c.fieldCaseFn(name)
		field.Type = c.convertType(typ)
		fields = append(fields, &field)
	}
	return
}

func (c *collector) mapOpsToEnumValues(ops []Op) (values []*EnumValue) {
	for _, op := range ops {
		value := EnumValue{}
		value.Name = c.fieldCaseFn(op.Name)
		value.Str = op.Op
		values = append(values, &value)
	}
	return
}

func (c *collector) mapStringsToEnumValues(strings []string) (values []*EnumValue) {
	for _, str := range strings {
		value := EnumValue{}
		value.Name = c.fieldCaseFn(str)
		value.Str = str
		values = append(values, &value)
	}
	return
}

type Defs struct {
	Defs       []Def
	Interfaces map[string]*Interface
	Structs    map[string]*Struct
	Enums      map[string]*Enum
}

func (d *Defs) push(def Def) {
	d.Defs = append(d.Defs, def)
	switch def := def.(type) {
	case *Interface:
		d.Interfaces[def.Name] = def
	case *Struct:
		d.Structs[def.Name] = def
	case *Enum:
		d.Enums[def.Name] = def
	}
}

func CollectDefinition(list *List, fieldCaseFn func(string) string, typeCaseFn func(string) string) (defs *Defs, err error) {
	defs = &Defs{
		Interfaces: make(map[string]*Interface),
		Structs:    make(map[string]*Struct),
		Enums:      make(map[string]*Enum),
	}
	defs.Defs = make([]Def, 0)
	c := collector{
		fieldCaseFn:   fieldCaseFn,
		typeCaseFn:    typeCaseFn,
		interfacesMap: make(map[string]*Interface),
	}

	// collect interface
	for _, node := range list.Node {
		if len(node.OneOf) > 0 {
			name := c.typeCaseFn(node.NodeType)
			iface := &Interface{
				Name: name,
			}
			if len(node.BaseNodeType) > 0 {
				iface.Embed = c.typeCaseFn(node.BaseNodeType[0])
			}
			for _, field := range node.Body {
				iface.Fields = append(iface.Fields, &Field{
					Name: c.fieldCaseFn(field.Key),
					Type: c.convertType(field.Value),
					Tag:  field.Key,
				})
			}
			defs.push(iface)
			c.interfacesMap[node.NodeType] = iface
		}
	}

	// collect struct
	for _, node := range list.Node {
		if len(node.OneOf) > 0 {
			iface := c.interfacesMap[node.NodeType]
			for _, field := range node.Body {
				iface.Fields = append(iface.Fields, &Field{
					Name: c.fieldCaseFn(field.Key),
					Type: c.convertType(field.Value),
					Tag:  field.Key,
				})
			}
			continue
		}
		var body Struct
		body.NodeType = node.NodeType
		body.Name = c.typeCaseFn(node.NodeType)
		if len(node.BaseNodeType) > 0 {
			body.Implements = make([]string, len(node.BaseNodeType))
			for i, base := range node.BaseNodeType {
				body.Implements[i] = c.typeCaseFn(base)
			}
		}

		body.Fields = make([]*Field, 0, len(node.Body)+1)

		body.Fields = c.mapToStructFields(node.Body)

		// add loc field
		loc := Field{}
		loc.Name = "Loc"
		loc.Type = &Type{Name: "Loc"}
		loc.Tag = "loc"
		body.Fields = append(body.Fields, &loc)

		defs.push(&body)
	}

	//generate enum mapping of unary_op, binary_op, ident_usage, endian
	enum := &Enum{}
	enum.Name = "UnaryOp"
	enum.Values = c.mapOpsToEnumValues(list.UnaryOp)
	defs.push(enum)

	enum = &Enum{}
	enum.Name = "BinaryOp"
	enum.Values = c.mapOpsToEnumValues(list.BinaryOp)
	defs.push(enum)

	enum = &Enum{}
	enum.Name = "IdentUsage"
	enum.Values = c.mapStringsToEnumValues(list.IdentUsage)
	defs.push(enum)

	enum = &Enum{}
	enum.Name = "Endian"
	enum.Values = c.mapStringsToEnumValues(list.Endian)
	defs.push(enum)

	// scope definition
	scope := Struct{}
	scope.Name = "Scope"
	scope.Fields = c.mapToStructFields(list.Scope)
	defs.push(&scope)

	// loc definition
	loc := Struct{}
	loc.Name = "Loc"
	loc.Fields = make([]*Field, 0, len(list.Loc))
	loc.Fields = c.mapToStructFields(list.Loc)
	defs.push(&loc)

	// pos definition
	pos := Struct{}
	pos.Name = "Pos"
	pos.Fields = make([]*Field, 0, len(list.Pos))
	pos.Fields = c.mapToStructFields(list.Pos)
	defs.push(&pos)

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

func LoadFromJSON(r io.Reader) (list *List, err error) {
	list = &List{}
	err = json.NewDecoder(r).Decode(list)
	return
}

func LoadFromSrc2JSON(src2json string) (list *List, err error) {
	cmd := exec.Command(src2json, "--dump-ast", "--dump-enum-name")
	// get stdout
	stdout, err := cmd.StdoutPipe()
	err = cmd.Start()
	if err != nil {
		return nil, err
	}
	defer func() {
		if e := cmd.Wait(); e != nil {
			err = errors.Join(err, e)
		}
	}()
	list, err = LoadFromJSON(stdout)
	if err != nil {
		return nil, err
	}
	return
}

func DefaultSrc2JSONLocation() (path string, err error) {
	// execute command in executable directory (not current directory)
	//get executable directory
	path, err = os.Executable()
	if err != nil {
		log.Println(err)
		return
	}

	// exec "{path}/src2json --dump-ast" with exec package
	path = filepath.Join(filepath.Dir(path), "src2json")

	if runtime.GOOS == "windows" {
		path += ".exe"
	}
	return
}

func LoadFromDefaultSrc2JSON() (list *List, err error) {
	path, err := DefaultSrc2JSONLocation()
	if err != nil {
		return nil, err
	}
	return LoadFromSrc2JSON(path)
}
