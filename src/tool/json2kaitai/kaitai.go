package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"os"
	"path/filepath"

	"github.com/iancoleman/strcase"
	"github.com/on-keyday/brgen/ast2go/ast"
	"github.com/on-keyday/brgen/ast2go/gen"
	convert "sigs.k8s.io/yaml"
)

type Generator struct {
	seq int
}

func (g *Generator) NextSeq() int {
	g.seq++
	return g.seq
}

func (g *Generator) KaitaiExpr(e ast.Expr) string {
	switch v := e.(type) {
	case *ast.Ident:
		return v.Ident
	case *ast.Binary:
		return fmt.Sprintf("%s %s %s", g.KaitaiExpr(v.Left), v.Op, g.KaitaiExpr(v.Right))
	case *ast.IntLiteral:
		return v.Value
	case *ast.Cast:
		return g.KaitaiExpr(v.Expr)
	}
	return ""
}

func (g *Generator) IntType(i *ast.IntType) *string {
	str := ""
	if i.IsCommonSupported {
		if i.IsSigned {
			str += "s"
		} else {
			str += "u"
		}
		str += fmt.Sprintf("%d", *i.BitSize/8)
		if *i.BitSize != 8 {
			if i.Endian == ast.EndianBig {
				str += "be"
			} else if i.Endian == ast.EndianLittle {
				str += "le"
			} else {
				str += "be"
			}
		}
	} else {
		str = fmt.Sprintf("b%d", *i.BitSize)
	}

	return &str

}

func (g *Generator) FloatType(f *ast.FloatType) *string {
	str := ""
	switch *f.BitSize {
	case 32:
		str += "f4"
	case 64:
		str += "f8"
	}
	return &str
}

func (g *Generator) KaitaiType(t ast.Type) *string {
	switch v := t.(type) {
	case *ast.IdentType:
		i, _ := gen.LookupIdent(v.Ident)
		return &i.Ident
	case *ast.IntType:
		return g.IntType(v)
	case *ast.FloatType:
		return g.FloatType(v)
	}
	return nil
}

func (g *Generator) GenerateAttribute(c *Type, s *ast.StructType) error {
	c.Seq = []*Attribute{}
	members := s.Fields
	for _, member := range members {
		switch f := member.(type) {
		case *ast.Field:
			field := Attribute{}
			if f.Ident != nil {
				f.Ident.Ident = strcase.ToSnake(f.Ident.Ident)
				field.ID = &f.Ident.Ident
			}
			typ := f.FieldType
			if i_typ, ok := typ.(*ast.IdentType); ok {
				typ = i_typ.Base
			}
			if u, ok := typ.(*ast.StructUnionType); ok {
				field.Type = &TypeRef{
					Switch: &TypeSwitch{
						Cases: map[string]*TypeRef{},
					},
				}
				if u.Cond != nil {
					trueExpr := "true"
					field.Type.Switch.SwitchOn = &AnyScalar{
						String: &trueExpr,
					}
				} else {
					expr := g.KaitaiExpr(u.Cond)
					field.Type.Switch.SwitchOn = &AnyScalar{
						String: &expr,
					}
				}
				for i, s := range u.Structs {
					seq := g.NextSeq()
					name := fmt.Sprintf("anonymous%d", seq)
					newTy := &Type{}
					c.Types[name] = newTy
					err := g.GenerateAttribute(newTy, s)
					if err != nil {
						return err
					}
					expr := g.KaitaiExpr(u.Conds[i])
					field.Type.Switch.Cases[expr] = &TypeRef{
						String: g.KaitaiType(s),
					}
				}
				continue
			}
			if i_typ, ok := typ.(*ast.IntType); ok {
				field.Type = &TypeRef{
					String: g.IntType(i_typ),
				}
			}
			if i_typ, ok := typ.(*ast.EnumType); ok {
				field.Enum = &i_typ.Base.Ident.Ident
				field.Type = &TypeRef{
					String: g.KaitaiType(i_typ.Base.BaseType),
				}
			}
			if f_typ, ok := typ.(*ast.FloatType); ok {
				field.Type = &TypeRef{
					String: g.FloatType(f_typ),
				}
			}
			if arr, ok := typ.(*ast.ArrayType); ok {
				field.Type = &TypeRef{
					String: g.KaitaiType(arr.ElementType),
				}
				if arr.BitSize != nil && *arr.BitSize != 0 {
					val := fmt.Sprintf("%d", *arr.BitSize/8)
					field.Size = &StringOrInteger{
						String: &val,
					}
				}
				rep := g.KaitaiExpr(arr.Length)
				field.RepeatExpr = &StringOrInteger{
					String: &rep,
				}
				rep2 := Expr
				field.Repeat = &rep2
			}
			c.Seq = append(c.Seq, &field)
		}
	}
	return nil
}

func (g *Generator) Generate(id string, root *ast.Program) (*Type, error) {
	kaitai := &Type{}
	for _, element := range root.Elements {
		if enum, ok := element.(*ast.Enum); ok {
			if kaitai.Enums == nil {
				kaitai.Enums = map[string]EnumSpec{}
			}
			spec := EnumSpec{}
			enum.Ident.Ident = strcase.ToSnake(enum.Ident.Ident)
			for _, member := range enum.Members {
				val := g.KaitaiExpr(member.Value)
				spec[val] = member.Ident.Ident
			}
			kaitai.Enums[enum.Ident.Ident] = spec
		}
	}
	sorted := gen.TopologicalSortFormat(root)
	err := g.GenerateAttribute(kaitai, sorted[0].Body.StructType)
	if err != nil {
		return nil, err
	}
	kaitai.Meta = &Meta{
		ID: &Identifier{
			String: &id,
		},
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
	id := filepath.Base(file.Files[0])
	id = id[:len(id)-len(filepath.Ext(id))]
	g := &Generator{}
	src, err := g.Generate(id, prog)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		if src == nil {
			os.Exit(1)
		}
	}

	j, err := json.Marshal(src)

	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
		return
	}

	out, err := convert.JSONToYAML(j)

	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
		return
	}

	os.Stdout.Write(out)
}
