package main

import (
	"bytes"
	"encoding/json"
	"flag"
	"fmt"
	"go/format"
	"io"
	"os"
	"strings"

	ast2go "github.com/on-keyday/brgen/ast2go/ast"
	"github.com/on-keyday/brgen/ast2go/gen"
)

var f = flag.Bool("s", false, "tell spec of json2go")
var filename = flag.String("f", "", "file to parse")

type IndentPrinter struct {
	w      io.Writer
	indent int
}

func (i *IndentPrinter) Indent(plus int) {
	i.indent += plus
}

func (i *IndentPrinter) IndentAbs(abs int) (prev int) {
	prev = i.indent
	i.indent = abs
	return
}

func (i *IndentPrinter) IndentStr() string {
	return strings.Repeat("    ", i.indent)
}

func (i *IndentPrinter) Printf(format string, args ...interface{}) {
	fmt.Fprintf(i.w, i.IndentStr()+format, args...)
}

type BitFields struct {
	Size   uint64
	Fields []*ast2go.Field
	Name   string
}

type UnionStruct struct {
	Struct *ast2go.StructType
	Name   string
}

type Generator struct {
	w            io.Writer
	type_area    bytes.Buffer
	func_area    bytes.Buffer
	seq          int
	bitFields    map[*ast2go.Field]*BitFields
	unionStructs map[*ast2go.StructType]*UnionStruct
	indent       int
	imports      map[string]struct{}
	exprStringer *gen.ExprStringer
}

func NewGenerator(w io.Writer) *Generator {
	return &Generator{
		w:            w,
		bitFields:    make(map[*ast2go.Field]*BitFields),
		unionStructs: make(map[*ast2go.StructType]*UnionStruct),
		imports:      make(map[string]struct{}),
		exprStringer: gen.NewExprStringer(),
	}
}

func (g *Generator) Indent(plus int) {
	g.indent += plus
}

func (g *Generator) IndentStr() string {
	return strings.Repeat("    ", g.indent)
}

func (g *Generator) Printf(format string, args ...interface{}) {
	fmt.Fprintf(&g.type_area, g.IndentStr()+format, args...)
}

func (g *Generator) PrintfFunc(format string, args ...interface{}) {
	fmt.Fprintf(&g.func_area, g.IndentStr()+format, args...)
}

func (g *Generator) getSeq() int {
	g.seq++
	return g.seq
}

func (g *Generator) getType(typ ast2go.Type) string {
	if i_type, ok := typ.(*ast2go.IntType); ok {
		if i_type.IsCommonSupported {
			if i_type.IsSigned {
				return fmt.Sprintf("int%d", i_type.BitSize)
			} else {
				return fmt.Sprintf("uint%d", i_type.BitSize)
			}
		}
	}
	if e_type, ok := typ.(*ast2go.EnumType); ok {
		return fmt.Sprintf("%s", e_type.Base.Ident.Ident)
	}
	return ""
}

func MaxHexDigit(bitSize uint64) uint64 {
	maxValue := (1 << bitSize) - 1
	digit := uint64(0)
	for maxValue > 0 {
		maxValue >>= 4
		digit++
	}
	return digit
}

func (g *Generator) writeBitField(belong string, fields []*ast2go.Field, size uint64, intSet, simple bool) {
	if intSet && simple {
		seq := g.getSeq()
		g.Printf("flags%d uint%d\n", seq, size)

		offset := uint64(0)
		format := fmt.Sprintf("return ((t.flags%d & 0x%%0%dx) >> %%d)", seq, MaxHexDigit(size))
		for _, v := range fields {
			field_size := v.FieldType.GetBitSize()
			if field_size == 1 {
				g.PrintfFunc("func (t *%s) %s() bool {\n", belong, v.Ident.Ident)
			} else {
				g.PrintfFunc("func (t *%s) %s() uint%d {\n", belong, v.Ident.Ident, size)
			}
			shift := (size - field_size - offset)
			bitMask := ((1 << field_size) - 1)
			if field_size == 1 {
				g.PrintfFunc(format+"==1\n", bitMask<<shift, shift)
			} else {
				g.PrintfFunc(format+"\n", bitMask<<shift, shift)
			}
			g.PrintfFunc("}\n")
			offset += field_size
		}
		g.bitFields[fields[len(fields)-1]] = &BitFields{
			Size:   size,
			Fields: fields,
			Name:   fmt.Sprintf("flags%d", seq),
		}
		return
	}
}

func (g *Generator) writeStructType(belong string, s *ast2go.StructType) {
	var non_aligned []*ast2go.Field
	var (
		is_int_set        = true
		is_simple         = true
		size       uint64 = 0
	)
	for _, v := range s.Fields {
		if field, ok := v.(*ast2go.Field); ok {
			if field.BitAlignment == ast2go.BitAlignmentNotTarget {
				continue
			}
			typ := field.FieldType
			if i_typ, ok := typ.(*ast2go.IdentType); ok {
				typ = i_typ.Base
			}
			if field.BitAlignment != ast2go.BitAlignmentByteAligned {
				non_aligned = append(non_aligned, field)
				is_int_set = is_int_set && typ.GetIsIntSet()
				is_simple = is_simple &&
					(typ.GetNodeType() == ast2go.NodeTypeIntType ||
						typ.GetNodeType() == ast2go.NodeTypeEnumType)
				size += typ.GetBitSize()
				continue
			}
			if len(non_aligned) > 0 {
				non_aligned = append(non_aligned, field)
				is_int_set = is_int_set && typ.GetIsIntSet()
				is_simple = is_simple &&
					(typ.GetNodeType() == ast2go.NodeTypeIntType ||
						typ.GetNodeType() == ast2go.NodeTypeEnumType)
				size += typ.GetBitSize()
				g.writeBitField(belong, non_aligned, size, is_int_set, is_simple)
				non_aligned = nil
				is_int_set = true
				is_simple = true
				size = 0
				continue
			}
			if i_type, ok := typ.(*ast2go.IntType); ok {
				if i_type.IsCommonSupported {
					if i_type.IsSigned {
						g.Printf("%s int%d\n", field.Ident.Ident, i_type.BitSize)
					} else {
						g.Printf("%s uint%d\n", field.Ident.Ident, i_type.BitSize)
					}
					continue
				}
			}
			if e_type, ok := typ.(*ast2go.EnumType); ok {
				g.Printf("%s %s\n", field.Ident.Ident, e_type.Base.Ident.Ident)
				continue
			}
			if u, ok := typ.(*ast2go.StructUnionType); ok {
				for _, v := range u.Structs {
					seq := g.getSeq()
					g.Printf("union_%d_ struct {", seq)
					g.writeStructType(belong, v)
					g.Printf("}\n")
					g.unionStructs[v] = &UnionStruct{
						Struct: v,
						Name:   fmt.Sprintf("union_%d_", seq),
					}
				}
				for _, field := range u.UnionFields {
					typ := field.FieldType.(*ast2go.UnionType)
					if typ.CommonType != nil {
						typStr := g.getType(typ.CommonType)
						g.PrintfFunc("func (t *%s) %s() *%s {\n", belong, field.Ident.Ident, typStr)
						cond0 := ""
						if typ.Cond != nil {
							cond0 = g.exprStringer.ExprString(typ.Cond)
						} else {
							cond0 = "true"
						}
						hasElse := false
						for _, c := range typ.Candidates {
							writeReturn := func() {
								if c.Field != nil {
									s := g.unionStructs[c.Field.BelongStruct].Name
									g.PrintfFunc("tmp := %s(t.%s.%s)\n", typStr, s, c.Field.Ident.Ident)
									g.PrintfFunc("return &tmp\n")
								} else {
									g.PrintfFunc("return nil\n")
								}
							}
							if c.Cond != nil {
								cond := g.exprStringer.ExprString(c.Cond)
								if hasElse {
									g.PrintfFunc("else ")
								}
								g.PrintfFunc("if %s == %s {\n", cond0, cond)
								writeReturn()
								g.PrintfFunc("}")
								hasElse = true
							} else {
								g.PrintfFunc("else {\n")
								writeReturn()
								g.PrintfFunc("}")
							}
							g.PrintfFunc("\n")
						}
						g.PrintfFunc("return nil\n")
						g.PrintfFunc("}\n")
					}
				}
			}
		}
	}
}

func (g *Generator) writeFormat(p *ast2go.Format) {
	g.Printf("type %s struct {\n", p.Ident.Ident)
	g.Indent(1)
	g.writeStructType(p.Ident.Ident, p.Body.StructType)
	g.Indent(-1)
	g.Printf("}\n")
	g.writeEncode(p)
}

func (g *Generator) writeAppendUint(size uint64, field string) {
	if size == 8 {
		g.PrintfFunc("buf = append(buf, byte(t.%s))\n", field)
		return
	}
	g.imports["encoding/binary"] = struct{}{}
	g.PrintfFunc("buf = binary.BigEndian.AppendUint%d(buf, uint%d(t.%s))\n", size, size, field)
}

func (g *Generator) writeFieldEncode(p *ast2go.Field) {
	if p.BitAlignment != ast2go.BitAlignmentByteAligned {
		return
	}
	if b, ok := g.bitFields[p]; ok {
		g.writeAppendUint(b.Size, b.Name)
		return
	}
	typ := p.FieldType
	if i_typ, ok := typ.(*ast2go.IdentType); ok {
		typ = i_typ.Base
	}
	if i_type, ok := typ.(*ast2go.IntType); ok {
		if i_type.IsCommonSupported {
			g.writeAppendUint(i_type.BitSize, p.Ident.Ident)
			return
		}
	}
}

func (g *Generator) writeEncode(p *ast2go.Format) {
	g.PrintfFunc("func (t *%s) Encode() ([]byte,error) {\n", p.Ident.Ident)
	g.PrintfFunc("buf := make([]byte, 0, %d)\n", p.Body.StructType.GetBitSize()/8)
	for _, field := range p.Body.StructType.Fields {
		if field, ok := field.(*ast2go.Field); ok {
			g.writeFieldEncode(field)
		}
	}
	g.PrintfFunc("return buf,nil\n")
	g.PrintfFunc("}\n")
}

func (g *Generator) writeProgram(p *ast2go.Program) {
	conf := &gen.GenConfig{}
	conf.LookupGoConfig(p)
	if conf.PackageName == "" {
		conf.PackageName = "main"
	}

	for _, v := range p.Elements {
		if v, ok := v.(*ast2go.Format); ok {
			if v.Body.StructType.BitAlignment != ast2go.BitAlignmentByteAligned {
				continue
			}
			g.writeFormat(v)
		}
	}
	io.WriteString(g.w, "// Code generated by brgen. DO NOT EDIT.\n")
	fmt.Fprintf(g.w, "package %s\n", conf.PackageName)
	if len(g.imports) > 0 {
		fmt.Fprintf(g.w, "import (\n")
		for k := range g.imports {
			fmt.Fprintf(g.w, "\t%q\n", k)
		}
		fmt.Fprintf(g.w, ")\n")
	}
	g.w.Write(g.type_area.Bytes())
	g.w.Write(g.func_area.Bytes())
}

func (g *Generator) Generate(file *ast2go.AstFile) error {
	p, err := ast2go.ParseAST(file.Ast)
	if err != nil {
		return err
	}
	g.writeProgram(p)
	return nil
}

func main() {
	flag.Parse()
	if *f {
		fmt.Println(`{
			"langs" : ["go"],
			"pass_by" : "stdin",
			"suffix" : ".go"
		}`)
		return
	}
	file := ast2go.AstFile{}
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
	buf := bytes.NewBuffer(nil)
	g := NewGenerator(buf)

	err := g.Generate(&file)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
		return
	}

	src, err := format.Source(buf.Bytes())
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
		return
	}
	os.Stdout.Write(src)
}
