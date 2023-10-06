package main

import (
	"encoding/json"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"runtime"

	"github.com/kita127/goconvcase"
)

type Node struct {
	NodeType     string            `json:"node_type"`
	BaseNodeType []string          `json:"base_node_type"`
	OneOf        []string          `json:"one_of"`
	Body         map[string]string `json:"body"`
}

type List struct {
	Node       []Node            `json:"node"`
	UnaryOp    []string          `json:"unary_op"`
	BinaryOp   []string          `json:"binary_op"`
	IdentUsage []string          `json:"ident_usage"`
	Endian     []string          `json:"endian"`
	Scope      map[string]string `json:"scope"`
	Loc        map[string]any    `json:"loc"`
}

type BodyNode struct {
	NodeType     string          `json:"node_type"`
	BaseNodeType []string        `json:"base_node_type"`
	Loc          string          `json:"loc"`
	Body         json.RawMessage `json:"body"`
}

type OneOfNode struct {
	NodeType string   `json:"node_type"`
	OneOf    []string `json:"one_of"`
}

type Def interface {
	def()
}

type Interface struct {
	DefName string
	Method  string
}

func (d *Interface) def() {}

type Field struct {
	Name string
	Type string
	Tag  string
}

type Struct struct {
	DefName    string
	Implements []string
	Fields     []*Field
}

type Enum struct {
	DefName string
	Values  []string
}

func (d *Struct) def() {}
func (d *Enum) def()   {}

var (
	sharedPtr = regexp.MustCompile("shared_ptr<(.*)>")
	weakPtr   = regexp.MustCompile("weak_ptr<(.*)>")
	array     = regexp.MustCompile("array<(.*)>")
)

func convertType(typ string, interfacesMap map[string]*Interface) (string, error) {
	arrayMatch := array.FindStringSubmatch(typ)
	prefix := ""
	if len(arrayMatch) > 0 {
		typ = arrayMatch[1]
		prefix += "[]"
	}
	sharedPtrMatch := sharedPtr.FindStringSubmatch(typ)
	if len(sharedPtrMatch) > 0 {
		typ = sharedPtrMatch[1]
		if _, ok := interfacesMap[typ]; !ok {
			prefix += "*"
		}
	}
	weakPtrMatch := weakPtr.FindStringSubmatch(typ)
	if len(weakPtrMatch) > 0 {
		typ = weakPtrMatch[1]
		if _, ok := interfacesMap[typ]; !ok {
			prefix += "*"
		}
	}
	typ, err := goconvcase.ConvertCase(typ, goconvcase.LowerSnake, goconvcase.UpperCamel)
	if err != nil {
		return "", err
	}
	typ = prefix + typ
	return typ, nil
}

func main() {
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

	cmd := exec.Command(path, "--dump-ast")

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
	defs := make([]Def, 0)
	var interfacesMap map[string]*Interface

	// collect interface
	for _, node := range list.Node {
		if len(node.OneOf) > 0 {
			oneof, err := goconvcase.ConvertCase(node.NodeType, goconvcase.LowerSnake, goconvcase.UpperCamel)
			if err != nil {
				log.Println(err)
				return
			}
			iface := &Interface{
				DefName: oneof,
				Method:  "is" + oneof,
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
		body.DefName, err = goconvcase.ConvertCase(node.NodeType, goconvcase.LowerSnake, goconvcase.UpperCamel)
		if err != nil {
			log.Println(err)
			return
		}
		if len(node.BaseNodeType) > 0 {
			body.Implements = make([]string, len(node.BaseNodeType))
			for i, base := range node.BaseNodeType {
				body.Implements[i], err = goconvcase.ConvertCase(base, goconvcase.LowerSnake, goconvcase.UpperCamel)
				body.Implements[i] = "is" + body.Implements[i]
				if err != nil {
					log.Println(err)
					return
				}
			}
		}

		body.Fields = make([]*Field, 0, len(node.Body)+1)

		// add loc field
		loc := Field{}
		loc.Name = "Loc"
		loc.Type = "Loc"
		loc.Tag = "`json:\"loc\"`"
		body.Fields = append(body.Fields, &loc)

		for name, typ := range node.Body {
			field := Field{}
			field.Tag = "`json:\"" + name + "\"`"
			field.Name, err = goconvcase.ConvertCase(name, goconvcase.LowerSnake, goconvcase.UpperCamel)
			if err != nil {
				log.Println(err)
				return
			}
			field.Type, err = convertType(typ, interfacesMap)
			if err != nil {
				log.Println(err)
				return
			}
			body.Fields = append(body.Fields, &field)
		}

		defs = append(defs, &body)
	}

	//generate enum mapping of unary_op, binary_op, ident_usage, endian
	enum := Enum{}
	enum.DefName = "UnaryOp"
	for _, unaryOp := range list.UnaryOp {
		enum.Values = append(enum.Values, unaryOp)
	}
	defs = append(defs, &enum)

	enum = Enum{}
	enum.DefName = "BinaryOp"
	for _, binaryOp := range list.BinaryOp {
		enum.Values = append(enum.Values, binaryOp)
	}
	defs = append(defs, &enum)

	enum = Enum{}
	enum.DefName = "IdentUsage"
	for _, identUsage := range list.IdentUsage {
		enum.Values = append(enum.Values, identUsage)
	}
	defs = append(defs, &enum)

	enum = Enum{}
	enum.DefName = "Endian"
	for _, endian := range list.Endian {
		enum.Values = append(enum.Values, endian)
	}
	defs = append(defs, &enum)

	// scope definition
	scope := Struct{}
	scope.DefName = "Scope"
	scope.Fields = make([]*Field, 0, len(list.Scope))
	for name, typ := range list.Scope {
		field := Field{}
		field.Name, err = goconvcase.ConvertCase(name, goconvcase.LowerSnake, goconvcase.UpperCamel)
		if err != nil {
			log.Println(err)
			return
		}
		field.Type, err = convertType(typ, interfacesMap)
		if err != nil {
			log.Println(err)
			return
		}
		scope.Fields = append(scope.Fields, &field)
	}

	// loc definition
	loc := Struct{}
	loc.DefName = "Loc"
	loc.Fields = make([]*Field, 0, len(list.Loc))
	for name, typ := range list.Loc {
		field := Field{}
		field.Name, err = goconvcase.ConvertCase(name, goconvcase.LowerSnake, goconvcase.UpperCamel)
		if err != nil {
			log.Println(err)
			return
		}
		field.Type, err = convertType(typ.(string), interfacesMap)
		if err != nil {
			log.Println(err)
			return
		}
		loc.Fields = append(loc.Fields, &field)
	}

}
