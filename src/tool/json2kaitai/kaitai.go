package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"os"

	"github.com/on-keyday/brgen/ast2go/ast"
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
	Seq []*Attribute `yaml:"seq"`
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
	str += fmt.Sprintf("%d", *i.BitSize/8)
	return Reference(str)
}

func GenerateAttribute(s *ast.Format) ([]*Attribute, error) {
	attr := []*Attribute{}
	members := s.Body.StructType.Fields
	for _, member := range members {
		switch f := member.(type) {
		case *ast.Field:
			if f.Ident == nil {
				continue
			}
			field := &Attribute{
				ID: f.Ident.Ident,
			}
			if i_typ, ok := f.FieldType.(*ast.IntType); ok {
				field.Type = IntType(i_typ)
			}
			if arr, ok := f.FieldType.(*ast.ArrayType); ok {
				if arr.BitSize != nil && *arr.BitSize != 0 {
					field.Size = fmt.Sprintf("%d", *arr.BitSize/8)
				}
			}
			attr = append(attr, field)
		}
	}
	return attr, nil
}

func Generate(root *ast.Program) (*Kaitai, error) {
	kaitai := &Kaitai{}
	var rootTypes []*ast.Format
	refMap := map[*ast.Format]int{}
	for _, member := range root.StructType.Fields {
		switch f := member.(type) {
		case *ast.Format:
			rootTypes = append(rootTypes, f)
			refCount := 0
			for _, member := range f.Body.StructType.Fields {
				switch f := member.(type) {
				case *ast.Field:
					typ := f.FieldType
					if ident_ty, ok := typ.(*ast.IdentType); ok {
						typ = ident_ty.Base
					}
					if _, ok := typ.(*ast.StructType); ok {
						refCount++
					}
				}
			}
			refMap[f] = refCount
		}
	}
	for _, fmt := range rootTypes {
		for _, member := range fmt.Body.StructType.Fields {
			switch f := member.(type) {
			case *ast.Field:
				typ := f.FieldType
				if ident_ty, ok := typ.(*ast.IdentType); ok {
					typ = ident_ty.Base
				}
				if struct_ty, ok := typ.(*ast.StructType); ok {
					refMap[fmt] += refMap[struct_ty.Base.(*ast.Format)] - 1
				}
			}
		}
	}
	// max reference
	var maxRef *ast.Format
	max := 0
	for fmt, ref := range refMap {
		if maxRef == nil {
			maxRef = fmt
			max = ref
		} else if ref > max {
			max = ref
			maxRef = fmt
		}
	}
	var err error
	kaitai.Seq, err = GenerateAttribute(maxRef)
	if err != nil {
		return nil, err
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
			"input" : "stdin",
			"suffix" : [".ksy"]
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
