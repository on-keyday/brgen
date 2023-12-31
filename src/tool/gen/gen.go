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

type EnumValue struct {
	Name  string `json:"name"`
	Value string `json:"value"`
}

type EnumValues []*EnumValue

type List struct {
	Node   []Node                                 `json:"node"`
	Enum   OrderedKeyList[EnumValues]             `json:"enum"`
	Struct OrderedKeyList[OrderedKeyList[string]] `json:"struct"`
}

type Def interface {
	def()
}

type Interface struct {
	Name           string
	Embed          string
	Fields         []*Field // common fields
	UnCommonFields []*Field // uncommon fields
	Derived        []string
}

func (d *Interface) def() {}

type Type struct {
	Name        string
	IsInterface bool
	IsPtr       bool
	IsArray     bool
	IsWeak      bool
	IsOptional  bool
}

func (d *Type) GoString() string {
	prefix := ""
	if d.IsArray {
		prefix += "[]"
	}
	if d.IsPtr {
		prefix += "*"
	}
	if !d.IsArray && !d.IsPtr && d.IsOptional {
		prefix += "*"
	}
	return prefix + d.Name
}

func (d *Type) RustString() string {
	prefix := ""
	postfix := ""
	if d.IsArray {
		prefix += "Vec<"
		postfix += ">"
	}
	if d.IsPtr {
		ptr := "Rc<"
		if d.IsWeak {
			ptr = "Weak<"
		}
		prefix += ptr + "RefCell<"
		postfix += ">" + ">"
		if !d.IsArray {
			prefix = "Option<" + prefix
			postfix = postfix + ">"
		}
	}
	if d.IsInterface && !d.IsArray {
		prefix += "Option<"
		postfix += ">"
		if d.IsWeak {
			postfix = "Weak" + postfix
		}
	}
	if d.IsOptional {
		prefix = "Option<" + prefix
		postfix = postfix + ">"
	}
	return prefix + d.Name + postfix
}

func (d *Type) TsString() string {
	postfix := ""
	if d.IsArray {
		postfix += "[]"
	} else if d.IsPtr || d.IsInterface {
		postfix += "|null"
	}
	if d.IsOptional {
		postfix += "|null"
	}
	return d.Name + postfix
}

func (d *Type) PyString() string {
	prefix := ""
	postfix := ""
	if d.IsOptional {
		prefix += "Optional["
		postfix += "]"
	}
	if d.IsArray {
		prefix += "List["
		postfix += "]"
	} else if d.IsPtr || d.IsInterface {
		prefix += "Optional["
		postfix += "]"
	}
	return prefix + d.Name + postfix
}

func (d *Type) CString() string {
	postfix := ""
	if (d.IsOptional || d.IsArray) && d.Name == "uintptr_t" {
		return "void*"
	}
	if d.IsInterface || d.IsPtr {
		postfix += "*"
	} else if !d.IsArray && d.IsOptional {
		postfix += "*"
	}
	if d.IsArray {
		postfix += "*"
	}
	return d.Name + postfix
}

func (d *Type) CSharpString() string {
	prefix := ""
	postfix := ""
	if d.IsArray {
		prefix += "List<"
		postfix += ">?"
	} else if d.IsPtr || d.IsInterface || d.IsOptional {
		postfix += "?"
	}
	return prefix + d.Name + postfix
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
	if d.IsOptional {
		prefix += "*"
	}
	return prefix + d.Name
}

type Field struct {
	Name string
	Type *Type
	Tag  string
}

type Struct struct {
	NodeType       string
	Name           string
	Implements     []string
	Fields         []*Field
	UnCommonFields []*Field
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
	optional  = regexp.MustCompile("optional<(.*)>")
)

type collector struct {
	fieldCaseFn   func(string) string
	typeCaseFn    func(string) string
	primitiveMap  map[string]string
	interfacesMap map[string]*Interface
}

func (c *collector) convertType(typ string) *Type {
	arrayMatch := array.FindStringSubmatch(typ)
	isPtr := false
	isArray := false
	isInterface := false
	isWeak := false
	isOptional := false
	optionalMatch := optional.FindStringSubmatch(typ)
	if len(optionalMatch) > 0 {
		typ = optionalMatch[1]
		isOptional = true
	}
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
		isWeak = true
		if _, ok := c.interfacesMap[typ]; ok {
			isInterface = true
		} else {
			isPtr = true
		}
	}

	if prim, ok := c.primitiveMap[typ]; ok {
		typ = prim
	} else if typ != "string" && typ != "uint" && typ != "bool" && typ != "uintptr" && typ != "any" {
		typ = c.typeCaseFn(typ)
	}
	return &Type{
		Name:        typ,
		IsInterface: isInterface,
		IsPtr:       isPtr,
		IsArray:     isArray,
		IsWeak:      isWeak,
		IsOptional:  isOptional,
	}
}

func (c *collector) mapToStructFields(m OrderedKeyList[string], field ...*Field) (fields []*Field) {
	fields = field
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

func (c *collector) mapOpsToEnumValues(ops EnumValues) (values []*EnumValue) {
	for _, op := range ops {
		value := EnumValue{}
		value.Name = c.fieldCaseFn(op.Name)
		value.Value = op.Value
		values = append(values, &value)
	}
	return
}

func (c *collector) mapStringsToEnumValues(strings []string) (values []*EnumValue) {
	for _, str := range strings {
		value := EnumValue{}
		value.Name = c.fieldCaseFn(str)
		value.Value = str
		values = append(values, &value)
	}
	return
}

type Defs struct {
	NodeTypes  []string
	Defs       []Def
	Interfaces map[string]*Interface
	Structs    map[string]*Struct
	Enums      map[string]*Enum
	ScopeDef   *Struct
}

func (d *Defs) push(def Def) {
	d.Defs = append(d.Defs, def)
	switch def := def.(type) {
	case *Interface:
		d.Interfaces[def.Name] = def
	case *Struct:
		d.Structs[def.Name] = def
		if def.Name == "Scope" {
			d.ScopeDef = def
		}
	case *Enum:
		d.Enums[def.Name] = def
	}
}

func CollectDefinition(list *List, fieldCaseFn func(string) string, typeCaseFn func(string) string, primitiveMap map[string]string) (defs *Defs, err error) {
	defs = &Defs{
		NodeTypes:  make([]string, 0, len(list.Node)),
		Interfaces: make(map[string]*Interface),
		Structs:    make(map[string]*Struct),
		Enums:      make(map[string]*Enum),
	}
	defs.Defs = make([]Def, 0)
	c := collector{
		fieldCaseFn:   fieldCaseFn,
		typeCaseFn:    typeCaseFn,
		interfacesMap: make(map[string]*Interface),
		primitiveMap:  primitiveMap,
	}

	//generate enum mappings

	for _, enum := range list.Enum {
		name := c.typeCaseFn(enum.Key)
		defs.push(&Enum{
			Name:   name,
			Values: c.mapOpsToEnumValues(enum.Value),
		})
	}

	// collect interface
	for _, node := range list.Node {
		defs.NodeTypes = append(defs.NodeTypes, node.NodeType)
		if len(node.OneOf) > 0 {
			name := c.typeCaseFn(node.NodeType)
			iface := &Interface{
				Name: name,
			}
			if len(node.BaseNodeType) > 0 {
				iface.Embed = c.typeCaseFn(node.BaseNodeType[0])
			}
			for _, oneof := range node.OneOf {
				iface.Derived = append(iface.Derived, c.typeCaseFn(oneof))
			}
			defs.push(iface)
			c.interfacesMap[node.NodeType] = iface
		}
	}

	// collect struct
	for _, node := range list.Node {
		if len(node.OneOf) > 0 {
			iface := c.interfacesMap[node.NodeType]
			iface.Fields = append(iface.Fields, &Field{
				Name: c.fieldCaseFn("Loc"),
				Type: &Type{Name: c.typeCaseFn("Loc")},
				Tag:  "loc",
			})
			for _, field := range node.Body {
				iface.Fields = append(iface.Fields, &Field{
					Name: c.fieldCaseFn(field.Key),
					Type: c.convertType(field.Value),
					Tag:  field.Key,
				})
			}
			if iface.Embed != "" {
				emb, ok := defs.Interfaces[iface.Embed]
				if ok {
					common := map[string]struct{}{}
					for _, field := range emb.Fields {
						common[field.Name] = struct{}{}
					}
					for _, field := range iface.Fields {
						if _, ok := common[field.Name]; !ok {
							iface.UnCommonFields = append(iface.UnCommonFields, field)
						}
					}
				} else {
					iface.UnCommonFields = iface.Fields
				}
			} else {
				iface.UnCommonFields = iface.Fields
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

		// add loc field
		loc := Field{}
		loc.Name = c.fieldCaseFn("Loc")
		loc.Type = &Type{Name: c.typeCaseFn("Loc")}
		loc.Tag = "loc"

		body.Fields = c.mapToStructFields(node.Body, &loc)

		if len(body.Implements) != 0 {
			if iface, ok := defs.Interfaces[body.Implements[0]]; ok {
				common := map[string]struct{}{}
				for _, field := range iface.Fields {
					common[field.Name] = struct{}{}
				}
				for _, field := range body.Fields {
					if _, ok := common[field.Name]; !ok {
						body.UnCommonFields = append(body.UnCommonFields, field)
					}
				}
			} else {
				body.UnCommonFields = body.Fields
			}
		} else {
			body.UnCommonFields = body.Fields
		}

		defs.push(&body)
	}

	//generate struct mappings
	for _, struct_ := range list.Struct {
		name := c.typeCaseFn(struct_.Key)
		s := &Struct{
			Name:   name,
			Fields: c.mapToStructFields(struct_.Value),
		}
		s.UnCommonFields = s.Fields
		defs.push(s)
	}

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
	cmd := exec.Command(src2json, "--dump-types")
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
