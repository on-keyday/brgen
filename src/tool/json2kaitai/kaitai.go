package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"os"

	"github.com/on-keyday/brgen/ast2go/ast"
	"github.com/on-keyday/brgen/ast2go/gen"
	"gopkg.in/yaml.v3"
)

func KaitaiExpr(e ast.Expr) string {
	if i_lit := e.(*ast.IntLiteral); i_lit != nil {
		return i_lit.Value
	}
	if i_lit := e.(*ast.Ident); i_lit != nil {
		return i_lit.Ident
	}
	if b := e.(*ast.Binary); b != nil {
		return fmt.Sprintf("%s %s %s", KaitaiExpr(b.Left), b.Op.String(), KaitaiExpr(b.Right))
	}
	return ""
}

func IntType(i *ast.IntType) *string {
	str := ""
	if i.IsCommonSupported {
		if i.IsSigned {
			str += "s"
		} else {
			str += "u"
		}
		str += fmt.Sprintf("%d", *i.BitSize/8)
	} else {
		str = fmt.Sprintf("b%d", *i.BitSize)
	}

	return &str

}

func FloatType(f *ast.FloatType) *string {
	str := ""
	switch *f.BitSize {
	case 32:
		str += "f4"
	case 64:
		str += "f8"
	}
	return &str
}

func GenerateAttribute(s *ast.Format) ([]Attribute, error) {
	attr := []Attribute{}
	members := s.Body.StructType.Fields
	for _, member := range members {
		switch f := member.(type) {
		case *ast.Field:
			field := &Attribute{}
			if f.Ident != nil {
				field.ID = &f.Ident.Ident
			}
			if i_typ, ok := f.FieldType.(*ast.IntType); ok {
				field.Type = &TypeRef{
					String: IntType(i_typ),
				}

			}
			if f_typ, ok := f.FieldType.(*ast.FloatType); ok {
				field.Type = &TypeRef{
					String: FloatType(f_typ),
				}
			}
			if arr, ok := f.FieldType.(*ast.ArrayType); ok {
				if arr.BitSize != nil && *arr.BitSize != 0 {
					val := fmt.Sprintf("%d", *arr.BitSize/8)
					field.Size.String = &val
				}
				rep := KaitaiExpr(arr.Length)
				field.RepeatExpr.String = &rep
			}
			attr = append(attr, *field)
		}
	}
	return attr, nil
}

func Generate(root *ast.Program) (*Coordinate, error) {
	kaitai := &Coordinate{}
	sorted := gen.TopologicalSortFormat(root)
	var err error
	kaitai.Seq, err = GenerateAttribute(sorted[0])
	if err != nil {
		return nil, err
	}
	kaitai.Meta = &Meta{}
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
