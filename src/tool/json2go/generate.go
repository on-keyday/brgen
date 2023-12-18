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

var f = flag.Bool("s", false, "tell spec of json2go")
var filename = flag.String("f", "", "file to parse")
var usePut = flag.Bool("use-put", false, "use PutUintXXX instead of AppendUintXXX")
var decodeReturnsLen = flag.Bool("decode-returns-len", true, "func Decode returns length of read bytes")
var useMustEncode = flag.Bool("must-encode", true, "add MustEncode func")
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

func resetFlag() {
	*f = false
	*filename = ""
	*usePut = false
	*decodeReturnsLen = true
	*useMustEncode = true
	mappingWords = map[string]string{}
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
	if arr_type, ok := typ.(*ast2go.ArrayType); ok {
		if arr_type.Length != nil && arr_type.Length.GetConstantLevel() == ast2go.ConstantLevelConstant {
			len := g.exprStringer.ExprString(arr_type.Length)
			return fmt.Sprintf("[%s]%s", len, g.getType(arr_type.BaseType))
		}
		return fmt.Sprintf("[]%s", g.getType(arr_type.BaseType))
	}
	if enum_type, ok := typ.(*ast2go.EnumType); ok {
		return fmt.Sprintf("%s", enum_type.Base.Ident.Ident)
	}
	if struct_type, ok := typ.(*ast2go.StructType); ok {
		if !struct_type.Recursive {
			return fmt.Sprintf("%s", struct_type.Base.(*ast2go.Format).Ident.Ident)
		}
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
		format := fmt.Sprintf("((t.flags%d & 0x%%0%dx) >> %%d)", seq, MaxHexDigit(size))
		for _, v := range fields {
			field_size := v.FieldType.GetBitSize()
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
				g.PrintfFunc("t.flags%d |= uint%d(0x%x)\n", seq, size, bitMask<<shift)
				g.PrintfFunc("} else {\n")
				g.PrintfFunc("t.flags%d &= ^uint%d(0x%x)\n", seq, size, (bitMask << shift))
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
					g.PrintfFunc("t.flags%d = (t.flags%d & ^uint%d(0x%x)) | ((uint%d(v) & 0x%x) << %d)\n", seq, seq, size, bitMask<<shift, size, bitMask, shift)
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
					g.PrintfFunc("t.flags%d = (t.flags%d & ^uint%d(0x%x)) | ((v & 0x%x) << %d)\n", seq, seq, size, bitMask<<shift, bitMask, shift)
					g.PrintfFunc("return true\n")
					g.PrintfFunc("}\n")
				}
			}
			offset += field_size
			if field_size == 1 {
				g.exprStringer.IdentMapper[v.Ident.Ident] = fmt.Sprintf("(func() uint%d { if t.%s() {return 1}else {return 0}}())", size, v.Ident.Ident)
			} else {
				g.exprStringer.IdentMapper[v.Ident.Ident] = fmt.Sprintf("t.%s()", v.Ident.Ident)
			}
		}
		g.bitFields[fields[len(fields)-1]] = &BitFields{
			Size:   size,
			Fields: fields,
			Name:   fmt.Sprintf("flags%d", seq),
		}
		g.exprStringer.IdentMapper[fmt.Sprintf("flags%d", seq)] = fmt.Sprintf("t.flags%d", seq)
		return
	}
}

func (g *Generator) writeStructUnion(belong string, u *ast2go.StructUnionType) {
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
				collect := g.exprStringer.CollectDefine(typ.Cond)
				for _, v := range collect {
					g.writeSingleNode(v, false)
				}
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
					collect := g.exprStringer.CollectDefine(c.Cond)
					for _, v := range collect {
						g.writeSingleNode(v, false)
					}
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
			}
			g.PrintfFunc("\n")
			g.PrintfFunc("return nil\n")
			g.PrintfFunc("}\n")
			g.exprStringer.IdentMapper[field.Ident.Ident] = fmt.Sprintf("(*t.%s())", field.Ident.Ident)
		}
	}

}

func (g *Generator) writeStructType(belong string, s *ast2go.StructType) {
	var non_aligned []*ast2go.Field
	var (
		is_int_set        = true
		is_simple         = true
		size       uint64 = 0
	)
	for i, v := range s.Fields {
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
			if arr_type, ok := typ.(*ast2go.ArrayType); ok {
				typ := g.getType(arr_type)
				g.Printf("%s %s\n", field.Ident.Ident, typ)
				g.exprStringer.IdentMapper[field.Ident.Ident] = "t." + field.Ident.Ident
				if _, ok := arr_type.Length.(*ast2go.Range); ok {
					if field.EventualFollow == ast2go.FollowEnd {
						size := uint64(0)
						for k := i + 1; k < len(s.Fields); k++ {
							if f, ok := s.Fields[k].(*ast2go.Field); ok {
								if f.BitAlignment == ast2go.BitAlignmentNotTarget {
									continue
								}
								size += f.FieldType.GetBitSize()
							}
						}
						g.laterSize[field] = size
					}
				}
			}
			if i_type, ok := typ.(*ast2go.IntType); ok {
				typ := g.getType(i_type)
				g.Printf("%s %s\n", field.Ident.Ident, typ)
				g.exprStringer.IdentMapper[field.Ident.Ident] = "t." + field.Ident.Ident
				continue
			}
			if e_type, ok := typ.(*ast2go.EnumType); ok {
				typ := g.getType(e_type)
				g.Printf("%s %s\n", field.Ident.Ident, typ)
				g.exprStringer.IdentMapper[field.Ident.Ident] = "t." + field.Ident.Ident
				continue
			}
			if u, ok := typ.(*ast2go.StructUnionType); ok {
				g.writeStructUnion(belong, u)
			}
			if u, ok := typ.(*ast2go.StructType); ok {
				typ := g.getType(u)
				g.Printf("%s %s\n", field.Ident.Ident, typ)
				g.exprStringer.IdentMapper[field.Ident.Ident] = "t." + field.Ident.Ident
			}
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
	g.Printf("type %s struct {\n", p.Ident.Ident)
	g.writeStructType(p.Ident.Ident, p.Body.StructType)
	g.Printf("}\n")
	g.writeStructVisitor(p.Ident.Ident, p.Body.StructType)
	g.writeEncode(p)
	g.writeDecode(p)
}

func (g *Generator) writeAppendUint(size uint64, field string) {
	if size == 8 {
		g.PrintfFunc("buf = append(buf, byte(%s))\n", field)
		return
	}
	g.imports["encoding/binary"] = struct{}{}
	if *usePut {
		tmp := fmt.Sprintf("tmp%d", g.getSeq())
		g.PrintfFunc("%s := [%d]byte{}\n", tmp, size/8)
		g.PrintfFunc("binary.BigEndian.PutUint%d(%s[:], uint%d(%s))\n", size, tmp, size, field)
		g.PrintfFunc("buf = append(buf, %s[:]...)\n", tmp)
	} else {
		g.PrintfFunc("buf = binary.BigEndian.AppendUint%d(buf, uint%d(%s))\n", size, size, field)
	}
}

func (g *Generator) writeFieldEncode(p *ast2go.Field) {
	if p.BitAlignment != ast2go.BitAlignmentByteAligned {
		return
	}
	if b, ok := g.bitFields[p]; ok {
		converted := g.exprStringer.ExprString(&ast2go.Ident{Ident: b.Name})
		g.writeAppendUint(b.Size, converted)
		return
	}
	typ := p.FieldType
	if i_typ, ok := typ.(*ast2go.IdentType); ok {
		typ = i_typ.Base
	}
	if i_type, ok := typ.(*ast2go.IntType); ok {
		if i_type.IsCommonSupported {
			converted := g.exprStringer.ExprString(p.Ident)
			g.writeAppendUint(i_type.BitSize, converted)
			return
		}
	}
	if arr_type, ok := typ.(*ast2go.ArrayType); ok {
		if i_typ, ok := arr_type.BaseType.(*ast2go.IntType); ok && i_typ.BitSize == 8 {
			converted := g.exprStringer.ExprString(p.Ident)
			if arr_type.Length.GetConstantLevel() == ast2go.ConstantLevelConstant {
				g.PrintfFunc("buf = append(buf, %s[:]...)\n", converted)
				return
			}
			if _, ok := g.laterSize[p]; !ok {
				length := g.exprStringer.ExprString(arr_type.Length)
				g.PrintfFunc("len_%s := int(%s)\n", p.Ident.Ident, length)
				g.PrintfFunc("if len(%s) != len_%s {\n", converted, p.Ident.Ident)
				g.imports["fmt"] = struct{}{}
				g.PrintfFunc("return nil, fmt.Errorf(\"encode %s: expect %%d bytes but got %%d bytes\", len_%s, len(%s))\n", p.Ident.Ident, p.Ident.Ident, converted)
				g.PrintfFunc("}\n")
			}
			g.PrintfFunc("buf = append(buf, %s...)\n", converted)
			return
		}
	}
	if _, ok := typ.(*ast2go.StructType); ok {
		converted := g.exprStringer.ExprString(p.Ident)
		g.PrintfFunc("tmp_%s, err := %s.Encode()\n", p.Ident.Ident, converted)
		g.PrintfFunc("if err != nil {\n")
		g.imports["fmt"] = struct{}{}
		g.PrintfFunc("return nil, fmt.Errorf(\"encode %s: %%w\", err)\n", p.Ident.Ident)
		g.PrintfFunc("}\n")
		g.PrintfFunc("buf = append(buf, tmp_%s...)\n", p.Ident.Ident)
		return
	}
}

func (g *Generator) writeReadUint(size uint64, tmpName, field string, sign bool) {
	g.PrintfFunc("tmp%s := [%d]byte{}\n", tmpName, size/8)
	g.PrintfFunc("n_%s, err := r.Read(tmp%s[:])\n", tmpName, tmpName)
	g.PrintfFunc("if err != nil {\n")
	g.PrintfFunc("return err\n")
	g.PrintfFunc("}\n")
	g.PrintfFunc("if n_%s != %d {\n", tmpName, size/8)
	g.imports["fmt"] = struct{}{}
	if size == 8 {
		g.PrintfFunc("return fmt.Errorf(\"read %s: expect 1 byte but read %%d bytes\", n_%s)\n", tmpName, tmpName)
	} else {
		g.PrintfFunc("return fmt.Errorf(\"read %s: expect %d bytes but read %%d bytes\", n_%s)\n", tmpName, size/8, tmpName)
	}
	g.PrintfFunc("}\n")
	signStr := ""
	if !sign {
		signStr = "u"
	}
	if size == 8 {
		g.PrintfFunc("t.%s = %sint%d(tmp%s[0])\n", field, signStr, size, tmpName)
	} else {
		g.PrintfFunc("t.%s = %sint%d(binary.BigEndian.Uint%d(tmp%s[:]))\n", field, signStr, size, size, tmpName)
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
		g.writeReadUint(b.Size, b.Name, b.Name, false)
		return
	}
	typ := p.FieldType
	if i_typ, ok := typ.(*ast2go.IdentType); ok {
		typ = i_typ.Base
	}
	if p.Ident == nil {
		return
	}
	fieldName := p.Ident.Ident
	converted := fieldName
	if belongs, ok := g.unionStructs[p.BelongStruct]; ok {
		converted = fmt.Sprintf("%s.%s", belongs.Name, p.Ident.Ident)
	}
	if i_type, ok := typ.(*ast2go.IntType); ok {
		if i_type.IsCommonSupported {
			g.writeReadUint(i_type.BitSize, fieldName, converted, i_type.IsSigned)
			return
		}
	}
	if arr_type, ok := typ.(*ast2go.ArrayType); ok {
		if i_typ, ok := arr_type.BaseType.(*ast2go.IntType); ok && i_typ.BitSize == 8 {
			converted := "t." + converted
			length := g.exprStringer.ExprString(arr_type.Length)
			if arr_type.Length.GetConstantLevel() == ast2go.ConstantLevelConstant {
				g.PrintfFunc("n_%s, err := r.Read(%s[:])\n", p.Ident.Ident, converted)
				g.PrintfFunc("if err != nil {\n")
				g.PrintfFunc("return err\n")
				g.PrintfFunc("}\n")
				g.PrintfFunc("if n_%s != %s {\n", p.Ident.Ident, length)
				g.imports["fmt"] = struct{}{}
				g.PrintfFunc("return fmt.Errorf(\"read %s: expect %%d bytes but read %%d bytes\", %s, n_%s)\n", p.Ident.Ident, length, p.Ident.Ident)
				g.PrintfFunc("}\n")
				return
			}
			// use laterSize (like [..]u8)
			if size, ok := g.laterSize[p]; ok {
				g.PrintfFunc("r_seeker_tmp_%s, ok := r.(io.Seeker)\n", p.Ident.Ident)
				g.PrintfFunc("if !ok {\n")
				g.imports["fmt"] = struct{}{}
				g.PrintfFunc("return fmt.Errorf(\"read %s: expect io.Seeker but got %%T\", r)\n", p.Ident.Ident)
				g.PrintfFunc("}\n")
				g.PrintfFunc("// save current position\n")
				g.PrintfFunc("cur_tmp_%s, err := r_seeker_tmp_%s.Seek(0, io.SeekCurrent)\n", p.Ident.Ident, p.Ident.Ident)
				g.PrintfFunc("if err != nil {\n")
				g.PrintfFunc("return err\n")
				g.PrintfFunc("}\n")
				g.PrintfFunc("// seek to end to get remaining length\n")
				g.PrintfFunc("end_tmp_%s, err := r_seeker_tmp_%s.Seek(0, io.SeekEnd)\n", p.Ident.Ident, p.Ident.Ident)
				g.PrintfFunc("if err != nil {\n")
				g.PrintfFunc("return err\n")
				g.PrintfFunc("}\n")
				g.PrintfFunc("// restore position\n")
				g.PrintfFunc("_, err = r_seeker_tmp_%s.Seek(cur_tmp_%s, io.SeekStart)\n", p.Ident.Ident, p.Ident.Ident)
				g.PrintfFunc("if err != nil {\n")
				g.PrintfFunc("return err\n")
				g.PrintfFunc("}\n")
				g.PrintfFunc("// check remaining length is enough to read %d byte\n", size/8)
				g.PrintfFunc("if end_tmp_%s - cur_tmp_%s < %d {\n", p.Ident.Ident, p.Ident.Ident, size/8)
				g.imports["fmt"] = struct{}{}
				g.PrintfFunc("return fmt.Errorf(\"read %s: expect %%d bytes but got %%d bytes\", %d, end_tmp_%s - cur_tmp_%s)\n", p.Ident.Ident, size/8, p.Ident.Ident, p.Ident.Ident)
				g.PrintfFunc("}\n")
				length = fmt.Sprintf("(end_tmp_%s - cur_tmp_%s) - %d", p.Ident.Ident, p.Ident.Ident, size/8)
			}
			g.PrintfFunc("len_%s := int(%s)\n", p.Ident.Ident, length)
			g.PrintfFunc("if len_%s != 0 {\n", p.Ident.Ident)
			g.PrintfFunc("tmp%s := make([]byte, len_%s)\n", p.Ident.Ident, p.Ident.Ident)
			g.PrintfFunc("n_%s, err := r.Read(tmp%s[:])\n", p.Ident.Ident, p.Ident.Ident)
			g.PrintfFunc("if err != nil {\n")
			g.PrintfFunc("return err\n")
			g.PrintfFunc("}\n")
			g.PrintfFunc("if n_%s != len_%s {\n", p.Ident.Ident, p.Ident.Ident)
			g.imports["fmt"] = struct{}{}
			g.PrintfFunc("return fmt.Errorf(\"read %s: expect %%d bytes but read %%d bytes\", len_%s, n_%s)\n", p.Ident.Ident, p.Ident.Ident, p.Ident.Ident)
			g.PrintfFunc("}\n")
			g.PrintfFunc("%s = tmp%s[:]\n", converted, p.Ident.Ident)
			g.PrintfFunc("} else {\n")
			g.PrintfFunc("%s = nil\n", converted)
			g.PrintfFunc("}\n")
			return
		}
	}
	if _, ok := typ.(*ast2go.StructType); ok {
		converted := "t." + converted
		g.PrintfFunc("if err := %s.Read(r); err != nil {\n", converted)
		g.imports["fmt"] = struct{}{}
		g.PrintfFunc("return fmt.Errorf(\"read %s: %%w\", err)\n", p.Ident.Ident)
		g.PrintfFunc("}\n")
		return
	}
}

func (g *Generator) writeIf(if_ *ast2go.If, enc bool) {
	g.PrintfFunc("if %s {\n", g.exprStringer.ExprString(if_.Cond))
	for _, elem := range if_.Then.Elements {
		g.writeSingleNode(elem, enc)
	}
	for if_.Els != nil {
		if if_2, ok := if_.Els.(*ast2go.If); ok {
			g.PrintfFunc("} else if %s {\n", g.exprStringer.ExprString(if_2.Cond))
			for _, elem := range if_.Then.Elements {
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
	for _, elem := range m.Branch {
		if mb := elem.(*ast2go.MatchBranch); mb != nil {
			g.PrintfFunc("case %s:\n", g.exprStringer.ExprString(mb.Cond))
			g.writeSingleNode(mb.Then, enc)
		}
	}
	g.PrintfFunc("}\n")
}

func (g *Generator) writeSingleNode(node ast2go.Node, enc bool) {
	switch node.GetNodeType() {
	case ast2go.NodeTypeIndentBlock:
		indentBlock := node.(*ast2go.IndentBlock)
		for _, elem := range indentBlock.Elements {
			g.writeSingleNode(elem, enc)
		}
	case ast2go.NodeTypeIf:
		g.writeIf(node.(*ast2go.If), enc)
	case ast2go.NodeTypeMatch:
		g.writeMatch(node.(*ast2go.Match), enc)
	case ast2go.NodeTypeField:
		if enc {
			g.writeFieldEncode(node.(*ast2go.Field))
		} else {
			g.writeFieldDecode(node.(*ast2go.Field))
		}
	case ast2go.NodeTypeScopedStatement:
		g.writeSingleNode(node.(*ast2go.ScopedStatement).Statement, enc)
	case ast2go.NodeTypeBinary:
		binary := node.(*ast2go.Binary)
		if binary.Op == ast2go.BinaryOpDefineAssign {
			ident := binary.Left.(*ast2go.Ident)
			g.PrintfFunc("%s := %s\n", ident.Ident, g.exprStringer.ExprString(binary.Right))
		}
	}
}

func (g *Generator) writeEncode(p *ast2go.Format) {
	g.PrintfFunc("func (t *%s) Encode() ([]byte,error) {\n", p.Ident.Ident)
	g.PrintfFunc("buf := make([]byte, 0, %d)\n", p.Body.StructType.GetBitSize()/8)
	for _, elem := range p.Body.Elements {
		g.writeSingleNode(elem, true)
	}
	g.PrintfFunc("return buf,nil\n")
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

	if *decodeReturnsLen {
		g.PrintfFunc("func (t *%s) Decode(d []byte) (int,error) {\n", p.Ident.Ident)
		g.PrintfFunc("r := bytes.NewReader(d)\n")
		g.PrintfFunc("err := t.Read(r)\n")
		g.PrintfFunc("return int(int(r.Size()) - r.Len()),err\n")
	} else {
		g.PrintfFunc("func (t *%s) Decode(d []byte) error {\n", p.Ident.Ident)
		g.PrintfFunc("r := bytes.NewReader(d)\n")
		g.PrintfFunc("return t.Read(r)\n")
	}
	g.PrintfFunc("}\n")
}

func (g *Generator) writeEnum(v *ast2go.Enum) {
	if v.BaseType == nil || v.BaseType.GetBitSize() == 0 {
		return
	}
	i_type, ok := v.BaseType.(*ast2go.IntType)
	if !ok {
		return
	}
	if i_type.IsCommonSupported {
		g.Printf("type %s uint%d\n", v.Ident.Ident, v.BaseType.GetBitSize())
	} else {
		g.Printf("type %s int\n", v.Ident.Ident)
	}
	hasIota := false
	prevValue := ""
	g.Printf("const (\n")
	for _, m := range v.Members {
		if m.Expr != nil {
			value := g.exprStringer.ExprString(m.Expr)
			g.Printf("%s_%s %s = %s\n", v.Ident.Ident, m.Ident.Ident, v.Ident.Ident, value)
			hasIota = false
		} else {
			if !hasIota {
				if len(prevValue) > 0 {
					g.Printf("%s_%s %s = iota + %s\n", v.Ident.Ident, m.Ident.Ident, v.Ident.Ident, prevValue)
				} else {
					g.Printf("%s_%s %s = iota\n", v.Ident.Ident, m.Ident.Ident, v.Ident.Ident)
				}
				hasIota = true
			} else {
				g.Printf("%s_%s\n", v.Ident.Ident, m.Ident.Ident)
			}
		}
	}
	g.Printf(")\n")

	g.Printf("func (t %s) String() string {\n", v.Ident.Ident)
	g.Printf("switch t {\n")
	for _, m := range v.Members {
		g.Printf("case %s_%s:\n", v.Ident.Ident, m.Ident.Ident)
		g.Printf("return %q\n", m.Ident.Ident)
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
		if v, ok := v.(*ast2go.Format); ok {
			if v.Body.StructType.BitAlignment != ast2go.BitAlignmentByteAligned {
				continue
			}
			g.writeFormat(v)
		}
		if v, ok := v.(*ast2go.Enum); ok {
			g.writeEnum(v)
		}
		if v, ok := v.(*ast2go.Binary); ok && v.Op == ast2go.BinaryOpConstAssign &&
			v.Right.GetConstantLevel() == ast2go.ConstantLevelConstant {
			ident := v.Left.(*ast2go.Ident)
			g.Printf("const %s = %s\n", ident.Ident, g.exprStringer.ExprString(v.Right))
		}
	}
	io.WriteString(&g.output, "// Code generated by brgen. DO NOT EDIT.\n")
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
