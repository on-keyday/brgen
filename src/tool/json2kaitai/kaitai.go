package main

import (
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"os"

	"github.com/on-keyday/brgen/ast2go/ast"
	"github.com/on-keyday/brgen/ast2go/gen"
	"gopkg.in/yaml.v3"
)

type Type interface {
	String() string
}

type Reference string

func (r Reference) String() string {
	return string(r)
}

type Case map[string]Type

type SwitchType struct {
	SwitchOn string `yaml:"switch-on"`
	Cases    Case   `yaml:"cases"`
}

type Attribute struct {
	ID       string `yaml:"id"`
	Type     Type   `yaml:"type,omitempty"`
	Repeat   string `yaml:"repeat,omitempty"`
	Enum     string `yaml:"enum,omitempty"`
	Size     string `yaml:"size,omitempty"`
	Contents string `yaml:"contents,omitempty"`
}

type Meta struct {
	ID string `yaml:"id"`
}

type Kaitai struct {
	Seq   []*Attribute    `yaml:"seq"`
	Types map[string]Type `yaml:"types"`
}

func IntType(i *ast.IntType) Type {
	str := ""
	if i.IsSigned {
		str += "s"
	} else {
		str += "u"
	}
	switch i.Endian {
	case ast.EndianBig:
		str += "be"
	case ast.EndianLittle:
		str += "le"
	case ast.EndianUnspec:
		str += "be"
	}
	str += fmt.Sprintf("%d", i.BitSize/8)
	return Reference(str)
}

func GenerateAttribute(s *ast.Format) ([]*Attribute, error) {
	attr := []*Attribute{}
	members := s.Body.StructType.Fields
	for _, member := range members {
		switch f := member.(type) {
		case *ast.Field:
			field := &Attribute{
				ID: f.Ident.Ident,
			}
			if i_typ, ok := f.FieldType.(*ast.IntType); ok {
				field.Type = IntType(i_typ)

			}
		}
	}
	return attr, nil
}

func Generate(root *ast.Program) (*Kaitai, error) {
	kaitai := &Kaitai{}
	var rootTypes []ast.Member
	gen.ConfigFromProgram(root, func(configName string, asCall bool, args ...ast.Expr) error {
		if configName == "config.export" {
			if !asCall {
				return errors.New("config.export must be called")
			}
			for _, arg := range args {
				ident, ok := arg.(*ast.Ident)
				if !ok {
					return errors.New("config.export must have identifier as argument")
				}
				typ, ok := ident.Base.(ast.Member)
				if !ok {
					return errors.New("config.export must have type as argument")
				}
				rootTypes = append(rootTypes, typ)
			}
		}
		return nil
	})
	for _, typ := range rootTypes {
		switch a := typ.(type) {
		case *ast.Format:
			attr, err := GenerateAttribute(a)
			if err != nil {
				return nil, err
			}
			kaitai.Seq = append(kaitai.Seq, attr...)
		}
	}
	return kaitai, nil
}

var f = flag.Bool("s", false, "tell spec of json2go")
var filename = flag.String("f", "", "file to parse")

func main() {
	flag.Parse()
	if *f {
		fmt.Println(`{
			"langs" : ["kaitai-struct"],
			"pass_by" : "stdin",
			"suffix" : ".ksy"
		}`)
		return
	}
	file := ast.AstFile{}
	if *filename != "" {
		f, err := os.Open(*filename)
		if err != nil {
			fmt.Fprintln(os.Stderr, err)
			os.Exit(1)
			return
		}
		defer f.Close()
		err = json.NewDecoder(f).Decode(&file)
		if err != nil {
			fmt.Fprintln(os.Stderr, err)
			os.Exit(1)
			return
		}
	} else {
		err := json.NewDecoder(os.Stdin).Decode(&file)
		if err != nil {
			fmt.Fprintln(os.Stderr, err)
			os.Exit(1)
			return
		}
	}

	prog, err := ast.ParseAST(file.Ast)

	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
		return
	}

	src, err := Generate(prog)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		if src == nil {
			os.Exit(1)
		}
	}

	out, err := yaml.Marshal(src)

	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
		return
	}

	os.Stdout.Write(out)
}
