package main

import (
	"bytes"
	"crypto/sha1"
	"flag"
	"fmt"
	"go/format"
	"io"
	"math/rand"
	"strings"

	"github.com/iancoleman/strcase"
	ast2go "github.com/on-keyday/brgen/ast2go/ast"
	"github.com/on-keyday/brgen/ast2go/gen"
)

var spec = flag.Bool("s", false, "tell spec of json2go")
var filename = flag.Bool("f", false, "arg is filename")
var legacyStdin = flag.Bool("legacy-stdin", false, "use legacy stdin")

// var usePut = flag.Bool("use-put", false, "use PutUintXXX instead of AppendUintXXX")
var decodeExact = flag.Bool("decode-exact", true, "add func DecodeExact")
var useMustEncode = flag.Bool("must-encode", true, "add func MustEncode")
var testInfo = flag.Bool("test-info", false, "output test info")
var mappingWords = map[string]string{}

func init() {
	flag.Func("map-word", "add mapping words (A=B)", func(s string) error {
		key_value := strings.Split(s, "=")
		if len(key_value) != 2 {
			return fmt.Errorf("invalid format: %s", s)
		}
		mappingWords[key_value[0]] = key_value[1]
		return nil
	})
}

func ResetFlag() {
	*spec = false
	*filename = false
	//*usePut = false
	*decodeExact = true
	*useMustEncode = true
	mappingWords = map[string]string{}
}

type BitFields struct {
	Ident  *ast2go.Ident
	Size   uint64
	Fields []*ast2go.Field
}

type UnionStruct struct {
	Struct *ast2go.StructType
	Name   string
}

type Generator struct {
	output       bytes.Buffer
	type_area    bytes.Buffer
	func_area    bytes.Buffer
	seq          int
	bitFields    map[*ast2go.Field]*BitFields
	unionStructs map[*ast2go.StructType]*UnionStruct
	laterSize    map[*ast2go.Field]uint64
	imports      map[string]struct{}
	exprStringer *gen.ExprStringer
	visitorName  string

	StructNames []string
}

func NewGenerator() *Generator {
	return &Generator{
		bitFields:    make(map[*ast2go.Field]*BitFields),
		unionStructs: make(map[*ast2go.StructType]*UnionStruct),
		imports:      make(map[string]struct{}),
		exprStringer: gen.NewExprStringer(),
		laterSize:    make(map[*ast2go.Field]uint64),
	}
}

func (g *Generator) Printf(format string, args ...interface{}) {
	fmt.Fprintf(&g.type_area, format, args...)
}

func (g *Generator) PrintfFunc(format string, args ...interface{}) {
	fmt.Fprintf(&g.func_area, format, args...)
}

func (g *Generator) getSeq() int {
	g.seq++
	return g.seq
}

func (g *Generator) getType(typ ast2go.Type) string {
	return g.exprStringer.GetType(typ)
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

func (g *Generator) writeBitField(belong string, prefix string, fields []*ast2go.Field, size uint64, intSet, simple bool) {
	if intSet && simple {
		seq := g.getSeq()
		g.Printf("flags%d uint%d\n", seq, size)

		offset := uint64(0)
		format := fmt.Sprintf("((t.%sflags%d & 0x%%0%dx) >> %%d)", prefix, seq, MaxHexDigit(size))
		for _, v := range fields {
			field_size := *v.FieldType.GetBitSize()
			typ := v.FieldType
			if ident, ok := typ.(*ast2go.IdentType); ok {
				typ = ident.Base
			}
			isEnumTy := typ.GetNodeType() == ast2go.NodeTypeEnumType
			shift := (size - field_size - offset)
			bitMask := uint64((1 << field_size) - 1)
			if field_size == 1 {
				g.PrintfFunc("func (t *%s) %s() bool {\n", belong, v.Ident.Ident)
				g.PrintfFunc("return "+format+"==1\n", bitMask<<shift, shift)
				g.PrintfFunc("}\n")
				g.PrintfFunc("func (t *%s) Set%s(v bool) {\n", belong, v.Ident.Ident)
				g.PrintfFunc("if v {\n")
				g.PrintfFunc("t.%sflags%d |= uint%d(0x%x)\n", prefix, seq, size, bitMask<<shift)
				g.PrintfFunc("} else {\n")
				g.PrintfFunc("t.%sflags%d &= ^uint%d(0x%x)\n", prefix, seq, size, (bitMask << shift))
				g.PrintfFunc("}\n")
				g.PrintfFunc("}\n")
			} else {
				if isEnumTy {
					g.PrintfFunc("func (t *%s) %s() %s {\n", belong, v.Ident.Ident, g.getType(typ))
					g.PrintfFunc("return %s("+format+")\n", g.getType(typ), bitMask<<shift, shift)
					g.PrintfFunc("}\n")
					g.PrintfFunc("func (t *%s) Set%s(v %s) bool {\n", belong, v.Ident.Ident, g.getType(typ))
					g.PrintfFunc("if v > %d {\n", bitMask)
					g.PrintfFunc("return false\n")
					g.PrintfFunc("}\n")
					g.PrintfFunc("t.%sflags%d = (t.%sflags%d & ^uint%d(0x%x)) | ((uint%d(v) & 0x%x) << %d)\n", prefix, seq, prefix, seq, size, bitMask<<shift, size, bitMask, shift)
					g.PrintfFunc("return true\n")
					g.PrintfFunc("}\n")
				} else {
					g.PrintfFunc("func (t *%s) %s() uint%d {\n", belong, v.Ident.Ident, size)
					g.PrintfFunc("return "+format+"\n", bitMask<<shift, shift)
					g.PrintfFunc("}\n")
					g.PrintfFunc("func (t *%s) Set%s(v uint%d) bool {\n", belong, v.Ident.Ident, size)
					g.PrintfFunc("if v > %d {\n", bitMask)
					g.PrintfFunc("return false\n")
					g.PrintfFunc("}\n")
					g.PrintfFunc("t.%sflags%d = (t.%sflags%d & ^uint%d(0x%x)) | ((v & 0x%x) << %d)\n", prefix, seq, prefix, seq, size, bitMask<<shift, bitMask, shift)
					g.PrintfFunc("return true\n")
					g.PrintfFunc("}\n")
				}
			}
			offset += field_size
			if field_size == 1 {
				g.exprStringer.SetIdentMap(v.Ident, fmt.Sprintf("(func() uint%d { if %%s%s() {return 1}else {return 0}}())", size, v.Ident.Ident))
			} else {
				g.exprStringer.SetIdentMap(v.Ident, fmt.Sprintf("%%s%s()", v.Ident.Ident))
			}
		}
		bf := &BitFields{
			Ident: &ast2go.Ident{
				Ident: fmt.Sprintf("flags%d", seq),
				Base:  &ast2go.Field{},
			},
			Size:   size,
			Fields: fields,
		}
		g.bitFields[fields[len(fields)-1]] = bf
		g.exprStringer.SetIdentMap(bf.Ident, fmt.Sprintf("%%s%s", bf.Ident.Ident))
		return
	}
}

func (g *Generator) InsertOverflowCheck(target string, compareType string, limitType string) {
	g.PrintfFunc("if %s > %s(^%s(0)) {\n", target, compareType, limitType)
	g.PrintfFunc("return false\n")
	g.PrintfFunc("}\n")
}

func (g *Generator) maybeWriteLengthSet(toSet string, length ast2go.Expr) {
	ident, ok := length.(*ast2go.Ident)
	if ok {
		ident, _ := gen.LookupBase(ident)
		f, ok := ident.Base.(*ast2go.Field)
		if ok {
			setTo := g.exprStringer.ExprString(f.Ident)
			typ := g.getType(f.FieldType)
			cast := "int"
			if typ == "uint64" {
				toSet = fmt.Sprintf("uint64(%s)", toSet)
				cast = "uint64"
			}
			g.InsertOverflowCheck(toSet, cast, typ)
			g.PrintfFunc("%s = %s(%s)\n", setTo, typ, toSet)
		}
	}
}

func (g *Generator) writeStructUnion(belong string, prefix string, u *ast2go.StructUnionType) {
	for _, v := range u.Structs {
		seq := g.getSeq()
		s := fmt.Sprintf("union_%d_", seq)
		g.Printf("union_%d_ struct {", seq)
		g.writeStructType(belong, prefix+s+".", v)
		g.Printf("}\n")
		g.unionStructs[v] = &UnionStruct{
			Struct: v,
			Name:   s,
		}
	}
	for _, field := range u.UnionFields {
		typ := field.FieldType.(*ast2go.UnionType)
		if typ.CommonType != nil {
			// write getter func
			typStr := g.getType(typ.CommonType)
			g.PrintfFunc("func (t *%s) %s() *%s {\n", belong, field.Ident.Ident, typStr)
			var cond0 ast2go.Expr
			if typ.Cond != nil {
				collect := g.exprStringer.CollectDefine(typ.Cond)
				for _, v := range collect {
					g.writeSingleNode(v, false)
				}
				cond0 = typ.Cond
			} else {
				cond0 = &ast2go.BoolLiteral{
					ExprType: &ast2go.BoolType{},
					Value:    true,
				}
			}
			hasElse := false
			endElse := false
			for _, c := range typ.Candidates {
				writeReturn := func() {
					if c.Field != nil {
						ident := g.exprStringer.ExprString(c.Field.Ident)
						arr, ok := c.Field.FieldType.(*ast2go.ArrayType)
						if ok && arr.LengthValue != nil {
							g.PrintfFunc("tmp := %s(%s[:])\n", typStr, ident)
						} else {
							g.PrintfFunc("tmp := %s(%s)\n", typStr, ident)
						}
						g.PrintfFunc("return &tmp\n")
					} else {
						g.PrintfFunc("return nil\n")
					}
				}
				if c.Cond != nil {
					collect := g.exprStringer.CollectDefine(c.Cond)
					for _, v := range collect {
						g.writeSingleNode(v, false)
					}
					b := &ast2go.Binary{
						Op:    ast2go.BinaryOpEqual,
						Left:  cond0,
						Right: c.Cond,
					}
					if hasElse {
						g.PrintfFunc("else ")
					}
					cond := g.exprStringer.ExprString(b)
					g.PrintfFunc("if %s {\n", cond)
					writeReturn()
					g.PrintfFunc("}")
					hasElse = true
				} else {
					g.PrintfFunc("else {\n")
					writeReturn()
					g.PrintfFunc("}")
					endElse = true
				}
			}
			g.PrintfFunc("\n")
			if !endElse {
				g.PrintfFunc("return nil\n")
			}
			g.PrintfFunc("}\n")
			g.exprStringer.SetIdentMap(field.Ident, fmt.Sprintf("(*%%s%s())", field.Ident.Ident))

			// write setter func
			hasElse = false
			endElse = false
			g.PrintfFunc("func (t *%s) Set%s(v %s) bool {\n", belong, field.Ident.Ident, typStr)
			for _, c := range typ.Candidates {
				writeSet := func() {
					if c.Field != nil {
						s := g.exprStringer.ExprString(c.Field.Ident)
						fieldType := g.getType(c.Field.FieldType)
						if fieldType != typStr && c.Field.FieldType.GetNodeType() == ast2go.NodeTypeIntType {
							g.InsertOverflowCheck("v", typStr, fieldType)
						} else if arr, ok := c.Field.FieldType.(*ast2go.ArrayType); ok {
							g.maybeWriteLengthSet("len(v)", arr.Length)
						}
						g.PrintfFunc("%s = %s(v)\n", s, fieldType)
						g.PrintfFunc("return true\n")
					} else {
						g.PrintfFunc("return false\n")
					}
				}
				if c.Cond != nil {
					collect := g.exprStringer.CollectDefine(c.Cond)
					for _, v := range collect {
						g.writeSingleNode(v, false)
					}
					b := &ast2go.Binary{
						Op:    ast2go.BinaryOpEqual,
						Left:  cond0,
						Right: c.Cond,
					}
					if hasElse {
						g.PrintfFunc("else ")
					}
					cond := g.exprStringer.ExprString(b)
					g.PrintfFunc("if %s {\n", cond)
					writeSet()
					g.PrintfFunc("}")
					hasElse = true
				} else {
					g.PrintfFunc("else {\n")
					writeSet()
					g.PrintfFunc("}")
					endElse = true
				}
			}
			g.PrintfFunc("\n")
			if !endElse {
				g.PrintfFunc("return false\n")
			}
			g.PrintfFunc("}\n")
		}
	}

}

func (g *Generator) writeStructType(belong string, prefix string, s *ast2go.StructType) {
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
				is_int_set = is_int_set && typ.GetNonDynamicAllocation()
				is_simple = is_simple &&
					(typ.GetNodeType() == ast2go.NodeTypeIntType ||
						typ.GetNodeType() == ast2go.NodeTypeEnumType)
				size += *typ.GetBitSize()
				continue
			}
			if len(non_aligned) > 0 {
				non_aligned = append(non_aligned, field)
				is_int_set = is_int_set && typ.GetNonDynamicAllocation()
				is_simple = is_simple &&
					(typ.GetNodeType() == ast2go.NodeTypeIntType ||
						typ.GetNodeType() == ast2go.NodeTypeEnumType)
				s := typ.GetBitSize()
				if s != nil {
					size += *s
				}
				g.writeBitField(belong, prefix, non_aligned, size, is_int_set, is_simple)
				non_aligned = nil
				is_int_set = true
				is_simple = true
				size = 0
				continue
			}
			if u, ok := typ.(*ast2go.StructUnionType); ok {
				g.writeStructUnion(belong, prefix, u)
				continue
			}
			if field.Ident == nil {
				field.Ident = &ast2go.Ident{
					Ident: fmt.Sprintf("hiddenField%d", g.getSeq()),
					Base:  field,
				}
			}
			if arr_type, ok := typ.(*ast2go.ArrayType); ok {
				typ := g.getType(arr_type)
				g.Printf("%s %s\n", field.Ident.Ident, typ)
				g.exprStringer.SetIdentMap(field.Ident, "%s"+field.Ident.Ident)
				if gen.IsAnyRange(arr_type.Length) {
					if field.EventualFollow == ast2go.FollowEnd {
						g.laterSize[field] = field.BelongStruct.FixedTailSize
					}
				}
				if gen.IsOnNamedStruct(field) && arr_type.LengthValue == nil && !gen.IsAnyRange(arr_type.Length) {
					s := g.exprStringer.ExprString(field.Ident)
					g.PrintfFunc("func (t *%s) Set%s(v %s) bool {\n", belong, field.Ident.Ident, typ)
					g.maybeWriteLengthSet("len(v)", arr_type.Length)
					g.PrintfFunc("%s = v\n", s)
					g.PrintfFunc("return true\n")
					g.PrintfFunc("}\n")
				}
			}
			if i_type, ok := typ.(*ast2go.IntType); ok {
				typ := g.getType(i_type)
				g.Printf("%s %s\n", field.Ident.Ident, typ)
			}
			if e_type, ok := typ.(*ast2go.EnumType); ok {
				typ := g.getType(e_type)
				g.Printf("%s %s\n", field.Ident.Ident, typ)
			}
			if s_type, ok := typ.(*ast2go.StrLiteralType); ok {
				magic := s_type.Base.Value
				g.Printf("// %s (%d byte)\n", magic, *typ.GetBitSize()/8)
				g.exprStringer.SetIdentMap(field.Ident, magic)
				continue
			}
			if u, ok := typ.(*ast2go.StructType); ok {
				typ := g.getType(u)
				g.Printf("%s %s\n", field.Ident.Ident, typ)
			}
			g.exprStringer.SetIdentMap(field.Ident, "%s"+prefix+field.Ident.Ident)
		}
	}
}

func gen5RandomASCIIBasedOnNameHash(name string) string {
	hash := sha1.New()
	hash.Write([]byte(name))
	sum := hash.Sum(nil)
	s := rand.NewSource(int64(sum[0]) | int64(sum[1])<<8 | int64(sum[2])<<16 | int64(sum[3])<<24 | int64(sum[4])<<32 | int64(sum[5])<<40 | int64(sum[6])<<48 | int64(sum[7])<<56)
	r := rand.New(s)
	for i := 0; i < 5; i++ {
		rand := r.Intn(26) + 'A'
		sum[i] = byte(rand)
	}
	return string(sum[:5])
}

func (g *Generator) writeStructVisitor(name string, p *ast2go.StructType) {
	if g.visitorName == "" {
		// use 3 char random string
		g.visitorName = "Visitor" + gen5RandomASCIIBasedOnNameHash(name)
		g.Printf("type %s interface {\n", g.visitorName)
		g.Printf("Visit(v %s,name string,field any)\n", g.visitorName)
		g.Printf("}\n")
		g.Printf("type %sFunc func(v %s,name string,field any)\n", g.visitorName, g.visitorName)
		g.Printf("func (f %sFunc) Visit(v %s,name string,field any) {\n", g.visitorName, g.visitorName)
		g.Printf("f(v,name,field)\n")
		g.Printf("}\n")
	}
	g.PrintfFunc("func (t *%s) Visit(v %s) {\n", name, g.visitorName)
	for _, f := range p.Fields {
		if field, ok := f.(*ast2go.Field); ok {
			if field.Ident == nil {
				continue
			}
			expr := g.exprStringer.ExprString(field.Ident)
			expr = strings.Replace(expr, "(*", "(", 1)
			g.PrintfFunc("v.Visit(v,%q,%s)\n", field.Ident.Ident, expr)
		}
	}
	g.PrintfFunc("}\n")
}

func (g *Generator) writeFormat(p *ast2go.Format) {
	g.StructNames = append(g.StructNames, p.Ident.Ident)
	g.Printf("type %s struct {\n", p.Ident.Ident)
	g.writeStructType(p.Ident.Ident, "", p.Body.StructType)
	g.Printf("}\n")
	g.writeStructVisitor(p.Ident.Ident, p.Body.StructType)
	g.writeEncode(p)
	g.writeDecode(p)
}

func (g *Generator) writeAppendUint(size uint64, field string) {
	if size == 8 {
		g.PrintfFunc("if n, err := w.Write([]byte{byte(%s)}); err != nil || n != 1 {\n", field)
		g.imports["fmt"] = struct{}{}
		g.PrintfFunc("return fmt.Errorf(\"encode %s: %%w\", err)\n", field)
		g.PrintfFunc("}\n")
		//g.PrintfFunc("buf = append(buf, byte(%s))\n", field)
		return
	}
	if size == 24 {
		g.PrintfFunc("if n, err := w.Write([]byte{byte(%s>>16),byte(%s>>8),byte(%s)}); err != nil || n != 3 {\n", field, field, field)
		g.imports["fmt"] = struct{}{}
		g.PrintfFunc("return fmt.Errorf(\"encode %s: %%w\", err)\n", field)
		g.PrintfFunc("}\n")
		//g.PrintfFunc("buf = append(buf, byte(%s>>16),byte(%s>>8),byte(%s))\n", field, field, field)
		return
	}
	g.imports["encoding/binary"] = struct{}{}
	tmp := fmt.Sprintf("tmp%d", g.getSeq())
	g.PrintfFunc("%s := [%d]byte{}\n", tmp, size/8)
	g.PrintfFunc("binary.BigEndian.PutUint%d(%s[:], uint%d(%s))\n", size, tmp, size, field)
	g.PrintfFunc("if n, err := w.Write(%s[:]); err != nil || n != %d {\n", tmp, size/8)
	g.imports["fmt"] = struct{}{}
	g.PrintfFunc("return fmt.Errorf(\"encode %s: %%w\", err)\n", field)
	g.PrintfFunc("}\n")
}

func (g *Generator) writeTypeEncode(ident string, typ ast2go.Type, p *ast2go.Field) {
	if i_typ, ok := typ.(*ast2go.IdentType); ok {
		typ = i_typ.Base
	}
	if i_type, ok := typ.(*ast2go.IntType); ok {
		g.writeAppendUint(*i_type.BitSize, ident)
		return
	}
	if enum_type, ok := typ.(*ast2go.EnumType); ok {
		g.writeAppendUint(*enum_type.BitSize, ident)
		return
	}
	if arr_type, ok := typ.(*ast2go.ArrayType); ok {
		if i_typ, ok := arr_type.ElementType.(*ast2go.IntType); ok && *i_typ.BitSize == 8 {
			if arr_type.Length.GetConstantLevel() == ast2go.ConstantLevelConstant {
				g.PrintfFunc("if n,err := w.Write(%s[:]);err != nil || n != len(%s) {\n", ident, ident)
				g.imports["fmt"] = struct{}{}
				g.PrintfFunc("return fmt.Errorf(\"encode %s: %%w\", err)\n", p.Ident.Ident)
				g.PrintfFunc("}\n")
				return
				//g.PrintfFunc("buf = append(buf, %s[:]...)\n", ident)
				//return
			}
			if _, ok := g.laterSize[p]; !ok {
				length := g.exprStringer.ExprString(arr_type.Length)
				g.PrintfFunc("len_%s := int(%s)\n", p.Ident.Ident, length)
				g.PrintfFunc("if len(%s) != len_%s {\n", ident, p.Ident.Ident)
				g.imports["fmt"] = struct{}{}
				g.PrintfFunc("return fmt.Errorf(\"encode %s: expect %%d bytes but got %%d bytes\", len_%s, len(%s))\n", p.Ident.Ident, p.Ident.Ident, ident)
				g.PrintfFunc("}\n")
			}
			g.PrintfFunc("if n,err := w.Write(%s);err != nil || n != len(%s) {\n", ident, ident)
			g.imports["fmt"] = struct{}{}
			g.PrintfFunc("return fmt.Errorf(\"encode %s: %%w\", err)\n", p.Ident.Ident)
			g.PrintfFunc("}\n")
			//g.PrintfFunc("buf = append(buf, %s...)\n", ident)
			return
		} else {
			if _, ok := g.laterSize[p]; !ok {
				length := g.exprStringer.ExprString(arr_type.Length)
				g.PrintfFunc("len_%s := int(%s)\n", p.Ident.Ident, length)
				g.PrintfFunc("if len(%s) != len_%s {\n", ident, p.Ident.Ident)
				g.imports["fmt"] = struct{}{}
				g.PrintfFunc("return fmt.Errorf(\"encode %s: expect %%d bytes but got %%d bytes\", len_%s, len(%s))\n", p.Ident.Ident, p.Ident.Ident, ident)
				g.PrintfFunc("}\n")
			}
			g.PrintfFunc("for _, v := range %s {\n", ident)
			g.writeTypeEncode("v", arr_type.ElementType, p)
			g.PrintfFunc("}\n")
			return

		}
	}
	if _, ok := typ.(*ast2go.StructType); ok {
		//g.PrintfFunc("tmp_%s, err := %s.Encode()\n", p.Ident.Ident, ident)
		//g.PrintfFunc("if err != nil {\n")
		g.PrintfFunc("if err := %s.Write(w);err != nil {\n", ident)
		g.imports["fmt"] = struct{}{}
		g.PrintfFunc("return fmt.Errorf(\"encode %s: %%w\", err)\n", p.Ident.Ident)
		g.PrintfFunc("}\n")
		//g.PrintfFunc("buf = append(buf, tmp_%s...)\n", p.Ident.Ident)
		return
	}
	if _, ok := typ.(*ast2go.StrLiteralType); ok {
		g.PrintfFunc("if n,err := w.Write([]byte(%s));err != nil || n != len(%s) {\n", ident, ident)
		g.imports["fmt"] = struct{}{}
		g.PrintfFunc("return fmt.Errorf(\"encode %s: %%w\", err)\n", p.Ident.Ident)
		g.PrintfFunc("}\n")
		//g.PrintfFunc("buf = append(buf, []byte(%s)...)\n", ident)
	}
}

func (g *Generator) writeFieldEncode(p *ast2go.Field) {
	if p.BitAlignment != ast2go.BitAlignmentByteAligned {
		return
	}
	if b, ok := g.bitFields[p]; ok {
		converted := g.exprStringer.ExprString(b.Ident)
		g.writeAppendUint(b.Size, converted)
		return
	}
	ident := g.exprStringer.ExprString(p.Ident)
	var restore []func()
	defer func() {
		for _, f := range restore {
			f()
		}
	}()
	if p.Arguments != nil {
		if p.Arguments.SubByteLength != nil {
			len := g.exprStringer.ExprString(p.Arguments.SubByteLength)
			seq := g.getSeq()
			g.imports["bytes"] = struct{}{}
			g.PrintfFunc("new_buf_%d := bytes.NewBuffer(nil)\n", seq)
			g.PrintfFunc("old_buf_%d_w := w\n", seq)
			g.PrintfFunc("w = new_buf_%d\n", seq)
			restore = append(restore, func() {
				g.PrintfFunc("if new_buf_%d.Len() != int(%s) {\n", seq, len)
				g.imports["fmt"] = struct{}{}
				g.PrintfFunc("return fmt.Errorf(\"encode %s: expect %%d bytes but got %%d bytes\",new_buf_%d.Len(), int(%s))\n", p.Ident.Ident, seq, len)
				g.PrintfFunc("}\n")
				g.PrintfFunc("_, err = new_buf_%d.WriteTo(old_buf_%d_w)\n", seq, seq)
				g.PrintfFunc("if err != nil {\n")
				g.PrintfFunc("return err\n")
				g.PrintfFunc("}\n")
				g.PrintfFunc("w = old_buf_%d_w\n", seq)
			})
		}
	}
	g.writeTypeEncode(ident, p.FieldType, p)
}

func (g *Generator) writeReadUint(size uint64, tmpName, field string, sign bool, enumTy *string) {
	g.PrintfFunc("tmp%s := [%d]byte{}\n", tmpName, size/8)
	g.PrintfFunc("n_%s, err := io.ReadFull(r,tmp%s[:])\n", tmpName, tmpName)
	g.PrintfFunc("if err != nil {\n")
	g.PrintfFunc("if err == io.ErrUnexpectedEOF || n_%s != %d /*stdlib bug?*/ {\n", tmpName, size/8)
	g.imports["fmt"] = struct{}{}
	if size == 8 {
		g.PrintfFunc("return fmt.Errorf(\"read %s: %%w: expect 1 byte but read %%d bytes\",io.ErrUnexpectedEOF, n_%s)\n", tmpName, tmpName)
	} else {
		g.PrintfFunc("return fmt.Errorf(\"read %s: %%w: expect %d bytes but read %%d bytes\",io.ErrUnexpectedEOF, n_%s)\n", tmpName, size/8, tmpName)
	}
	g.PrintfFunc("}\n")
	g.PrintfFunc("return err\n")
	g.PrintfFunc("}\n")
	signStr := ""
	if !sign {
		signStr = "u"
	}
	castTo := ""
	if enumTy != nil {
		castTo = *enumTy
	} else {

		castTo = fmt.Sprintf("%sint%d", signStr, gen.AlignInt(size))
	}
	if size == 8 {
		g.PrintfFunc("%s = %s(tmp%s[0])\n", field, castTo, tmpName)
	} else if size == 24 {
		g.PrintfFunc("%s = %s(uint32(tmp%s[0])<<16 | uint32(tmp%s[1])<<8 | uint32(tmp%s[2]))\n", field, castTo, tmpName, tmpName, tmpName)
	} else {
		g.PrintfFunc("%s = %s(binary.BigEndian.Uint%d(tmp%s[:]))\n", field, castTo, size, tmpName)
	}
}

func (g *Generator) calcLengthInByteOfTailSize(size uint64, tmp string) string {
	g.PrintfFunc("r_seeker_tmp_%s, ok := r.(io.Seeker)\n", tmp)
	g.PrintfFunc("if !ok {\n")
	g.imports["fmt"] = struct{}{}
	g.PrintfFunc("return fmt.Errorf(\"read %s: expect io.Seeker but %%T not implement\", r)\n", tmp)
	g.PrintfFunc("}\n")
	g.PrintfFunc("// save current position\n")
	g.PrintfFunc("cur_tmp_%s, err := r_seeker_tmp_%s.Seek(0, io.SeekCurrent)\n", tmp, tmp)
	g.PrintfFunc("if err != nil {\n")
	g.PrintfFunc("return err\n")
	g.PrintfFunc("}\n")
	g.PrintfFunc("// seek to end to get remaining length\n")
	g.PrintfFunc("end_tmp_%s, err := r_seeker_tmp_%s.Seek(0, io.SeekEnd)\n", tmp, tmp)
	g.PrintfFunc("if err != nil {\n")
	g.PrintfFunc("return err\n")
	g.PrintfFunc("}\n")
	g.PrintfFunc("// restore position\n")
	g.PrintfFunc("_, err = r_seeker_tmp_%s.Seek(cur_tmp_%s, io.SeekStart)\n", tmp, tmp)
	g.PrintfFunc("if err != nil {\n")
	g.PrintfFunc("return err\n")
	g.PrintfFunc("}\n")
	g.PrintfFunc("// check remaining length is enough to read %d byte\n", size/8)
	g.PrintfFunc("if end_tmp_%s - cur_tmp_%s < %d {\n", tmp, tmp, size/8)
	g.imports["fmt"] = struct{}{}
	g.PrintfFunc("return fmt.Errorf(\"read %s: expect %d bytes but got %%d bytes\", end_tmp_%s - cur_tmp_%s)\n", tmp, size/8, tmp, tmp)
	g.PrintfFunc("}\n")
	return fmt.Sprintf("(end_tmp_%s - cur_tmp_%s) - %d", tmp, tmp, size/8)
}

func (g *Generator) writeTypeDecode(ident string, typ ast2go.Type, p *ast2go.Field) {
	if i_typ, ok := typ.(*ast2go.IdentType); ok {
		typ = i_typ.Base
	}
	if i_type, ok := typ.(*ast2go.IntType); ok {
		g.writeReadUint(*i_type.BitSize, p.Ident.Ident, ident, i_type.IsSigned, nil)
		return
	}
	if enum_type, ok := typ.(*ast2go.EnumType); ok {
		g.writeReadUint(*enum_type.BitSize, p.Ident.Ident, ident, false, &enum_type.Base.Ident.Ident)
		return
	}
	if arr_type, ok := typ.(*ast2go.ArrayType); ok {
		length := g.exprStringer.ExprString(arr_type.Length)
		if i_typ, ok := arr_type.ElementType.(*ast2go.IntType); ok && *i_typ.BitSize == 8 {
			if arr_type.Length.GetConstantLevel() == ast2go.ConstantLevelConstant {
				g.PrintfFunc("n_%s, err := io.ReadFull(r,%s[:])\n", p.Ident.Ident, ident)
				g.PrintfFunc("if err != nil {\n")
				g.PrintfFunc("if err == io.ErrUnexpectedEOF || n_%s != %s /*stdlib bug?*/ {\n", p.Ident.Ident, length)
				g.imports["fmt"] = struct{}{}
				g.PrintfFunc("return fmt.Errorf(\"read %s: %%w: expect %%d bytes but read %%d bytes\",io.ErrUnexpectedEOF, %s, n_%s)\n", p.Ident.Ident, length, p.Ident.Ident)
				g.PrintfFunc("}\n")
				g.PrintfFunc("return err\n")
				g.PrintfFunc("}\n")
				return
			}
			// use laterSize (like [..]u8)
			if size, ok := g.laterSize[p]; ok {
				if size != 0 {
					length = g.calcLengthInByteOfTailSize(size, p.Ident.Ident)
				} else {
					g.imports["bytes"] = struct{}{}
					g.PrintfFunc("bytes_buf_%s := &bytes.Buffer{}\n", p.Ident.Ident)
					g.PrintfFunc("if _, err := io.Copy(bytes_buf_%s, r); err != nil {\n", p.Ident.Ident)
					g.PrintfFunc("return err\n")
					g.PrintfFunc("}\n")
					g.PrintfFunc("%s = bytes_buf_%s.Bytes()\n", ident, p.Ident.Ident)
					return
				}
			}
			g.PrintfFunc("len_%s := int(%s)\n", p.Ident.Ident, length)
			g.PrintfFunc("if len_%s != 0 {\n", p.Ident.Ident)
			g.PrintfFunc("tmp%s := make([]byte, len_%s)\n", p.Ident.Ident, p.Ident.Ident)
			g.PrintfFunc("n_%s, err := io.ReadFull(r,tmp%s[:])\n", p.Ident.Ident, p.Ident.Ident)
			g.PrintfFunc("if err != nil {\n")
			g.PrintfFunc("if err == io.ErrUnexpectedEOF || n_%s != len_%s /*stdlib bug?*/ {\n", p.Ident.Ident, p.Ident.Ident)
			g.imports["fmt"] = struct{}{}
			g.PrintfFunc("return fmt.Errorf(\"read %s: %%w: expect %%d bytes but read %%d bytes\",io.ErrUnexpectedEOF, len_%s, n_%s)\n", p.Ident.Ident, p.Ident.Ident, p.Ident.Ident)
			g.PrintfFunc("}\n")
			g.PrintfFunc("return err\n")
			g.PrintfFunc("}\n")
			g.PrintfFunc("%s = tmp%s[:]\n", ident, p.Ident.Ident)
			g.PrintfFunc("} else {\n")
			g.PrintfFunc("%s = nil\n", ident)
			g.PrintfFunc("}\n")
			return
		} else {
			// use laterSize (like [..]u8)
			if size, ok := g.laterSize[p]; ok {
				lengthInByte := ""
				if p.FieldType == typ && p.Arguments != nil && p.Arguments.SubByteLength != nil {
					// at this time, r is *io.LimitedReader so we can use N of r
					lengthInByte = "r.(*io.LimitedReader).N"
				} else if size != 0 {
					lengthInByte = g.calcLengthInByteOfTailSize(size, p.Ident.Ident)
				} else {
					tmp := fmt.Sprintf("tmp_byte_scanner%d_", g.getSeq())
					old := fmt.Sprintf("old_r_%s", p.Ident.Ident)
					g.imports["bufio"] = struct{}{}
					g.PrintfFunc("%s := bufio.NewReader(r)\n", tmp)
					g.PrintfFunc("%s := r\n", old)
					g.PrintfFunc("r = %s\n", tmp)
					g.PrintfFunc("for {\n")
					g.PrintfFunc("_, err := %s.ReadByte()\n", tmp)
					g.PrintfFunc("if err != nil {\n")
					g.PrintfFunc("if err != io.EOF {\n")
					g.imports["fmt"] = struct{}{}
					g.PrintfFunc("return fmt.Errorf(\"read %s: %%w\", err)\n", p.Ident.Ident)
					g.PrintfFunc("}\n")
					g.PrintfFunc("break\n")
					g.PrintfFunc("}\n")
					g.PrintfFunc("if err := %s.UnreadByte(); err != nil {\n", tmp)
					g.imports["fmt"] = struct{}{}
					g.PrintfFunc("return fmt.Errorf(\"read %s: unexpected unread error: %%w\", err)\n", p.Ident.Ident)
					g.PrintfFunc("}\n")
					seq := g.getSeq()
					g.PrintfFunc("var tmp%d_ %s\n", seq, g.getType(arr_type.ElementType))
					g.writeTypeDecode(fmt.Sprintf("tmp%d_", seq), arr_type.ElementType, p)
					g.PrintfFunc("%s = append(%s, tmp%d_)\n", ident, ident, seq)
					g.PrintfFunc("}\n")
					g.PrintfFunc("r = %s\n", old)
					return
				}
				g.PrintfFunc("len_%s := int(%s)\n", p.Ident.Ident, lengthInByte)
				g.PrintfFunc("tmp%s := make([]byte, len_%s)\n", p.Ident.Ident, p.Ident.Ident)
				g.PrintfFunc("n_%s, err := io.ReadFull(r,tmp%s[:])\n", p.Ident.Ident, p.Ident.Ident)
				g.PrintfFunc("if err != nil {\n")
				g.PrintfFunc("if err == io.ErrUnexpectedEOF || n_%s != len_%s /*stdlib bug?*/ {\n", p.Ident.Ident, p.Ident.Ident)
				g.imports["fmt"] = struct{}{}
				g.PrintfFunc("return fmt.Errorf(\"read %s: %%w: expect %%d bytes but read %%d bytes\",io.ErrUnexpectedEOF, len_%s, n_%s)\n", p.Ident.Ident, p.Ident.Ident, p.Ident.Ident)
				g.PrintfFunc("}\n")
				g.PrintfFunc("return err\n")
				g.PrintfFunc("}\n")
				g.imports["bytes"] = struct{}{}
				g.PrintfFunc("range_tmp_%s := bytes.NewReader(tmp%s[:])\n", p.Ident.Ident, p.Ident.Ident)
				g.PrintfFunc("tmp_old_r_%s := r\n", p.Ident.Ident)
				g.PrintfFunc("r = range_tmp_%s\n", p.Ident.Ident)
				g.PrintfFunc("for range_tmp_%s.Len() > 0 {\n", p.Ident.Ident)
				seq := g.getSeq()
				g.PrintfFunc("var tmp%d_ %s\n", seq, g.getType(arr_type.ElementType))
				g.writeTypeDecode(fmt.Sprintf("tmp%d_", seq), arr_type.ElementType, p)
				g.PrintfFunc("%s = append(%s, tmp%d_)\n", ident, ident, seq)
				g.PrintfFunc("}\n")
				g.PrintfFunc("r = tmp_old_r_%s\n", p.Ident.Ident)
			} else {
				g.PrintfFunc("len_%s := int(%s)\n", p.Ident.Ident, length)
				tmpI := fmt.Sprintf("i_%d", g.getSeq())
				g.PrintfFunc("for %[2]s := 0; %[2]s < len_%[1]s; %[2]s++ {\n", p.Ident.Ident, tmpI)
				seq := g.getSeq()
				g.PrintfFunc("var tmp%d_ %s\n", seq, g.getType(arr_type.ElementType))
				g.writeTypeDecode(fmt.Sprintf("tmp%d_", seq), arr_type.ElementType, p)
				if arr_type.LengthValue != nil {
					g.PrintfFunc("%s[%s] = tmp%d_\n", ident, tmpI, seq)
				} else {
					g.PrintfFunc("%s = append(%s, tmp%d_)\n", ident, ident, seq)
				}
				g.PrintfFunc("}\n")
			}
		}
	}
	if _, ok := typ.(*ast2go.StructType); ok {
		g.PrintfFunc("if err := %s.Read(r); err != nil {\n", ident)
		g.imports["fmt"] = struct{}{}
		g.PrintfFunc("return fmt.Errorf(\"read %s: %%w\", err)\n", p.Ident.Ident)
		g.PrintfFunc("}\n")
		return
	}
	if t, ok := typ.(*ast2go.StrLiteralType); ok {
		tmp := fmt.Sprintf("tmp%d_", g.getSeq())
		g.PrintfFunc("%s := [%d]byte{}\n", tmp, *t.GetBitSize()/8)
		g.PrintfFunc("n_%s, err := io.ReadFull(r,%s[:])\n", tmp, tmp)
		g.PrintfFunc("if err != nil {\n")
		g.PrintfFunc("if err == io.ErrUnexpectedEOF || n_%s != %d /*stdlib bug?*/ {\n", tmp, *t.GetBitSize()/8)
		g.imports["fmt"] = struct{}{}
		g.PrintfFunc("return fmt.Errorf(\"read %s: %%w: expect %d bytes but read %%d bytes\",io.ErrUnexpectedEOF, n_%s)\n", p.Ident.Ident, *t.GetBitSize()/8, tmp)
		g.PrintfFunc("}\n")
		g.PrintfFunc("return err\n")
		g.PrintfFunc("}\n")
		g.PrintfFunc("if string(%s[:]) != %s {\n", tmp, t.Base.Value)
		g.imports["fmt"] = struct{}{}
		g.PrintfFunc("return fmt.Errorf(\"read %s: expect %%s but got %%s\", %s, %s[:])\n", p.Ident.Ident, t.Base.Value, tmp)
		g.PrintfFunc("}\n")
		return
	}
}

func (g *Generator) writeFieldDecode(p *ast2go.Field) {
	if p.BitAlignment == ast2go.BitAlignmentNotTarget {
		return
	}
	if p.BitAlignment != ast2go.BitAlignmentByteAligned {
		return
	}
	if b, ok := g.bitFields[p]; ok {
		g.writeReadUint(b.Size, b.Ident.Ident, "t."+b.Ident.Ident, false, nil)
		return
	}
	typ := p.FieldType
	ident := g.exprStringer.ExprString(p.Ident)
	var restore []func()
	defer func() {
		for _, f := range restore {
			f()
		}
	}()
	if p.Arguments != nil {
		if p.Arguments.SubByteLength != nil {
			len := g.exprStringer.ExprString(p.Arguments.SubByteLength)
			g.PrintfFunc("sub_byte_len_%s := int64(%s)\n", p.Ident.Ident, len)
			g.PrintfFunc("sub_byte_r_%s := io.LimitReader(r, int64(sub_byte_len_%s))\n", p.Ident.Ident, p.Ident.Ident)
			seq := g.getSeq()
			g.PrintfFunc("tmp_old_r_%s_%d := r\n", p.Ident.Ident, seq)
			g.PrintfFunc("r = sub_byte_r_%s\n", p.Ident.Ident)
			restore = append(restore, func() {
				g.PrintfFunc("if sub_byte_r_%s.(*io.LimitedReader).N != 0 {\n", p.Ident.Ident)
				g.imports["fmt"] = struct{}{}
				g.PrintfFunc("return fmt.Errorf(\"read %s: expect %%d bytes but got %%d bytes\", sub_byte_len_%s, sub_byte_len_%s - sub_byte_r_%s.(*io.LimitedReader).N)\n", p.Ident.Ident, p.Ident.Ident, p.Ident.Ident, p.Ident.Ident)
				g.PrintfFunc("}\n")
				g.PrintfFunc("r = tmp_old_r_%s_%d\n", p.Ident.Ident, seq)
			})
		}
	}
	g.writeTypeDecode(ident, typ, p)
}

func (g *Generator) writeIf(if_ *ast2go.If, enc bool) {
	g.PrintfFunc("if %s {\n", g.exprStringer.ExprString(if_.Cond))
	for _, elem := range if_.Then.Elements {
		g.writeSingleNode(elem, enc)
	}
	for if_.Els != nil {
		if if_2, ok := if_.Els.(*ast2go.If); ok {
			g.PrintfFunc("} else if %s {\n", g.exprStringer.ExprString(if_2.Cond))
			for _, elem := range if_2.Then.Elements {
				g.writeSingleNode(elem, enc)
			}
			if_ = if_2
		} else {
			g.PrintfFunc("} else {\n")
			indentBlock := if_.Els.(*ast2go.IndentBlock)
			for _, elem := range indentBlock.Elements {
				g.writeSingleNode(elem, enc)
			}
			break
		}
	}
	g.PrintfFunc("}\n")
}

func (g *Generator) writeMatch(m *ast2go.Match, enc bool) {
	g.PrintfFunc("switch %s {\n", g.exprStringer.ExprString(m.Cond))
	for _, mb := range m.Branch {
		if gen.IsAnyRange(mb.Cond) {
			g.PrintfFunc("default:\n")
		} else {
			g.PrintfFunc("case %s:\n", g.exprStringer.ExprString(mb.Cond))
		}
		g.writeSingleNode(mb.Then, enc)
	}
	g.PrintfFunc("}\n")
}

func (g *Generator) writeSingleNode(node ast2go.Node, enc bool) {
	switch n := node.(type) {
	case *ast2go.IndentBlock:
		for _, elem := range n.Elements {
			g.writeSingleNode(elem, enc)
		}
	case *ast2go.If:
		g.writeIf(n, enc)
	case *ast2go.Match:
		g.writeMatch(n, enc)
	case *ast2go.Field:
		if enc {
			g.writeFieldEncode(n)
		} else {
			g.writeFieldDecode(n)
		}
	case *ast2go.ScopedStatement:
		g.writeSingleNode(n.Statement, enc)
	case *ast2go.Binary:
		binary := n
		if binary.Op == ast2go.BinaryOpDefineAssign ||
			binary.Op == ast2go.BinaryOpConstAssign {
			ident := binary.Left.(*ast2go.Ident)
			g.PrintfFunc("%s := %s\n", ident.Ident, g.exprStringer.ExprString(binary.Right))
			g.exprStringer.SetIdentMap(ident, ident.Ident)
		}
	case *ast2go.ExplicitError:
		unc := n.Message.Value
		unc = strings.ReplaceAll(unc, "%", "%%")
		if len(n.Base.Arguments) > 1 {
			args := strings.Repeat("%v", len(n.Base.Arguments)-1)
			argv := ""
			for _, arg := range n.Base.Arguments[1:] {
				argv += "," + g.exprStringer.ExprString(arg)
			}
			unc += "+ \"" + args + "\"" + argv
		}
		if enc {
			g.PrintfFunc("return fmt.Errorf(%s)\n", unc)
		} else {
			g.PrintfFunc("return fmt.Errorf(%s)\n", unc)
		}
	}
}

func (g *Generator) writeEncode(p *ast2go.Format) {
	g.PrintfFunc("func (t *%s) Write(w io.Writer) (err error) {\n", p.Ident.Ident)
	for _, elem := range p.Body.Elements {
		g.writeSingleNode(elem, true)
	}
	g.PrintfFunc("return nil\n")
	g.PrintfFunc("}\n")

	g.PrintfFunc("func (t *%s) Encode() ([]byte,error) {\n", p.Ident.Ident)
	g.PrintfFunc("w := bytes.NewBuffer(make([]byte, 0, %d))\n", p.Body.StructType.FixedHeaderSize/8)
	g.PrintfFunc("if err := t.Write(w); err != nil {\n")
	g.PrintfFunc("return nil, err\n")
	g.PrintfFunc("}\n")
	g.PrintfFunc("return w.Bytes(), nil\n")
	g.PrintfFunc("}\n")

	if *useMustEncode {
		g.PrintfFunc("func (t *%s) MustEncode() []byte {\n", p.Ident.Ident)
		g.PrintfFunc("buf, err := t.Encode()\n")
		g.PrintfFunc("if err != nil {\n")
		g.PrintfFunc("panic(err)\n")
		g.PrintfFunc("}\n")
		g.PrintfFunc("return buf\n")
		g.PrintfFunc("}\n")
	}
}

func (g *Generator) writeDecode(p *ast2go.Format) {
	g.PrintfFunc("func (t *%s) Read(r io.Reader) (err error) {\n", p.Ident.Ident)
	g.imports["io"] = struct{}{}
	for _, elem := range p.Body.Elements {
		g.writeSingleNode(elem, false)
	}
	g.PrintfFunc("return nil\n")
	g.PrintfFunc("}\n")
	g.PrintfFunc("\n")
	g.imports["bytes"] = struct{}{}

	g.PrintfFunc("func (t *%s) Decode(d []byte) (int,error) {\n", p.Ident.Ident)
	g.PrintfFunc("r := bytes.NewReader(d)\n")
	g.PrintfFunc("err := t.Read(r)\n")
	g.PrintfFunc("return int(int(r.Size()) - r.Len()),err\n")
	g.PrintfFunc("}\n")
	if *decodeExact {
		g.PrintfFunc("func (t *%s) DecodeExact(d []byte) error {\n", p.Ident.Ident)
		g.PrintfFunc("if n,err := t.Decode(d);err != nil {\n")
		g.PrintfFunc("return err\n")
		g.PrintfFunc("} else if n != len(d) {\n")
		g.imports["fmt"] = struct{}{}
		g.PrintfFunc("return fmt.Errorf(\"decode %s: expect %%d bytes but got %%d bytes\", len(d), n)\n", p.Ident.Ident)
		g.PrintfFunc("}\n")
		g.PrintfFunc("return nil\n")
		g.PrintfFunc("}\n")
	}
}

func (g *Generator) writeEnum(v *ast2go.Enum) {
	i_type, ok := v.BaseType.(*ast2go.IntType)
	if ok && i_type.IsCommonSupported {
		g.Printf("type %s %s\n", v.Ident.Ident, g.getType(v.BaseType))
	} else {
		g.Printf("type %s int\n", v.Ident.Ident)
	}
	g.Printf("const (\n")
	for _, m := range v.Members {
		value := g.exprStringer.ExprString(m.Value)
		g.Printf("%s_%s %s = %s\n", v.Ident.Ident, m.Ident.Ident, v.Ident.Ident, value)
		g.exprStringer.SetIdentMap(m.Ident, "%s"+v.Ident.Ident+"_"+m.Ident.Ident)
	}
	g.Printf(")\n")

	g.Printf("func (t %s) String() string {\n", v.Ident.Ident)
	g.Printf("switch t {\n")
	for _, m := range v.Members {
		g.Printf("case %s_%s:\n", v.Ident.Ident, m.Ident.Ident)
		if m.StrLiteral != nil {
			g.Printf("return %s\n", m.StrLiteral.Value)
		} else {
			g.Printf("return %q\n", m.Ident.Ident)
		}
	}
	g.Printf("}\n")
	g.imports["fmt"] = struct{}{}
	g.Printf("return fmt.Sprintf(\"%s(%%d)\", t)\n", v.Ident.Ident)
	g.Printf("}\n")
}

func (g *Generator) writeProgram(p *ast2go.Program) {
	conf := &gen.GenConfig{}
	conf.LookupGoConfig(p)
	if conf.PackageName == "" {
		conf.PackageName = "main"
	}

	for _, v := range p.Elements {

		if v, ok := v.(*ast2go.Enum); ok {
			g.writeEnum(v)
		}
		if v, ok := v.(*ast2go.Binary); ok && v.Op == ast2go.BinaryOpConstAssign &&
			v.Right.GetConstantLevel() == ast2go.ConstantLevelConstant {
			ident := v.Left.(*ast2go.Ident)
			g.Printf("const %s = %s\n", ident.Ident, g.exprStringer.ExprString(v.Right))
			g.exprStringer.SetIdentMap(ident, "%s"+ident.Ident)
		}
	}
	formats := gen.TopologicalSortFormat(p)
	for _, v := range formats {
		if v.Body.StructType.BitAlignment != ast2go.BitAlignmentByteAligned {
			continue
		}
		g.writeFormat(v)
	}
	io.WriteString(&g.output, "// Code generated by json2go. DO NOT EDIT.\n")
	fmt.Fprintf(&g.output, "package %s\n", conf.PackageName)
	if len(g.imports) > 0 {
		fmt.Fprintf(&g.output, "import (\n")
		for k := range g.imports {
			fmt.Fprintf(&g.output, "\t%q\n", k)
		}
		fmt.Fprintf(&g.output, ")\n")
	}
	g.output.Write(g.type_area.Bytes())
	g.output.Write(g.func_area.Bytes())
}

func (g *Generator) Generate(file *ast2go.AstFile) error {
	p, err := ast2go.ParseAST(file.Ast)
	if err != nil {
		return err
	}
	ast2go.Walk(p, ast2go.VisitFn(func(v ast2go.Visitor, n ast2go.Node) bool {
		ast2go.Walk(n, v)
		if n, ok := n.(*ast2go.Ident); ok {
			n.Ident = strcase.ToCamel(n.Ident)
			for k, v := range mappingWords {
				n.Ident = strings.Replace(n.Ident, k, v, -1)
			}
		}
		return true
	}))
	g.exprStringer.TypeProvider = g.getType
	g.exprStringer.Receiver = "t."
	g.exprStringer.BinaryMapper[ast2go.BinaryOpEqual] = func(s *gen.ExprStringer, x, y ast2go.Expr) string {
		if gen.IsAnyRange(y) {
			return "true" // compare with .. or ..= is always true
		}
		if r, ok := y.(*ast2go.Range); ok {
			rty := r.ExprType.(*ast2go.RangeType)
			baseTyp := rty.BaseType
			if identTy, ok := baseTyp.(*ast2go.IdentType); ok {
				baseTyp = identTy.Base
			}
			if baseTyp.GetNodeType() == ast2go.NodeTypeIntType ||
				baseTyp.GetNodeType() == ast2go.NodeTypeEnumType {
				if r.Start != nil && r.End != nil {
					x := s.ExprString(x)
					seq := g.getSeq()
					return fmt.Sprintf("(func() bool { tmp%d_ := %s ; return %s <= tmp_%d_ && tmp%d_ <= %s }())", seq, x, s.ExprString(r.Start), seq, seq, s.ExprString(r.End))
				}
				if r.Start != nil {
					return fmt.Sprintf("%s <= %s", s.ExprString(r.Start), s.ExprString(x))
				}
				if r.End != nil {
					return fmt.Sprintf("%s <= %s", s.ExprString(x), s.ExprString(r.End))
				}
			}
		}
		return "(" + s.ExprString(x) + " == " + s.ExprString(y) + ")"
	}
	g.exprStringer.BinaryMapper[ast2go.BinaryOpNotEqual] = func(s *gen.ExprStringer, x, y ast2go.Expr) string {
		return "!" + s.BinaryMapper[ast2go.BinaryOpEqual](s, x, y)
	}
	g.func_area.Reset()
	g.type_area.Reset()
	g.output.Reset()
	g.writeProgram(p)
	return nil
}

func (g *Generator) GenerateAndFormat(file *ast2go.AstFile) ([]byte, error) {
	err := g.Generate(file)
	if err != nil {
		return nil, err
	}
	data, err := format.Source(g.output.Bytes())
	if err != nil {
		return g.output.Bytes(), err
	}
	return data, nil
}
