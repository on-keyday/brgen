package main

import (
	"bytes"
	"crypto/sha1"
	"flag"
	"fmt"
	"go/format"
	"io"
	"math/rand"
	"regexp"
	"strconv"
	"strings"

	"github.com/iancoleman/strcase"
	ast2go "github.com/on-keyday/brgen/astlib/ast2go/ast"
	"github.com/on-keyday/brgen/astlib/ast2go/gen"
)

var spec = flag.Bool("s", false, "tell spec of json2go")
var filename = flag.Bool("f", false, "arg is filename")
var legacyStdin = flag.Bool("legacy-stdin", false, "use legacy stdin")

// var usePut = flag.Bool("use-put", false, "use PutUintXXX instead of AppendUintXXX")
var decodeExact = flag.Bool("decode-exact", true, "add func DecodeExact")
var useMustEncode = flag.Bool("must-encode", true, "add func MustEncode")
var useVisitor = flag.Bool("visitor", true, "add visitor pattern and ToMap")
var jsonMarshal = flag.Bool("marshal-json", true, "add func MarshalJSON (require:-visitor=true)")
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

type PrefixedBitField struct {
	FieldName   string
	Candidates  *ast2go.UnionType
	PrefixSize  uint64
	MaxSize     uint64
	PrefixField *ast2go.Field
	UnionField  *ast2go.Field
}

type UnionStruct struct {
	Struct    *ast2go.StructType
	TypeName  string
	FieldName *ast2go.Ident
}

type Generator struct {
	output           bytes.Buffer
	type_area        *bytes.Buffer
	func_area        *bytes.Buffer
	seq              int
	bitFields        map[*ast2go.Field]*BitFields
	bitFieldPart     map[*ast2go.Field]struct{}
	unionStructs     map[*ast2go.StructType]*UnionStruct
	prefixedBitField map[*ast2go.StructUnionType]*PrefixedBitField
	laterSize        map[*ast2go.Field]uint64
	terminalPattern  map[*ast2go.Field]struct{}
	imports          map[string]struct{}
	globalAssigned   map[*ast2go.Binary]struct{}
	exprStringer     *gen.ExprStringer
	visitorName      string
	defaultEndian    ast2go.Endian
	StructNames      []string
}

func (g *Generator) decideEndian(p ast2go.Endian) ast2go.Endian {
	if p != ast2go.EndianUnspec {
		return p
	}
	return g.defaultEndian
}

func NewGenerator() *Generator {
	return &Generator{
		type_area:        &bytes.Buffer{},
		func_area:        &bytes.Buffer{},
		bitFields:        make(map[*ast2go.Field]*BitFields),
		bitFieldPart:     make(map[*ast2go.Field]struct{}),
		unionStructs:     make(map[*ast2go.StructType]*UnionStruct),
		prefixedBitField: make(map[*ast2go.StructUnionType]*PrefixedBitField),
		imports:          make(map[string]struct{}),
		exprStringer:     gen.NewExprStringer(),
		laterSize:        make(map[*ast2go.Field]uint64),
		terminalPattern:  make(map[*ast2go.Field]struct{}),
		globalAssigned:   make(map[*ast2go.Binary]struct{}),
		defaultEndian:    ast2go.EndianBig,
	}
}

func (g *Generator) Printf(format string, args ...interface{}) {
	fmt.Fprintf(g.type_area, format, args...)
}

func (g *Generator) PrintfFunc(format string, args ...interface{}) {
	fmt.Fprintf(g.func_area, format, args...)
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

func (g *Generator) sumFields(fields []*ast2go.Field) (sum *uint64, prefix uint64) {
	sum = new(uint64)
	for _, v := range fields {
		if n := v.FieldType.GetBitSize(); n != nil {
			*sum += *n
		} else {
			return nil, *sum
		}
	}
	return sum, *sum
}

func (g *Generator) writeFullBitField(w printer, belong string, prefix string, size uint64, fields []*ast2go.Field) {
	seq := g.getSeq()
	w.Printf("flags%d uint%d\n", seq, size)

	offset := uint64(0)
	format := fmt.Sprintf("((t.%sflags%d & 0x%%0%dx) >> %%d)", prefix, seq, MaxHexDigit(size))
	for i, v := range fields {
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
		if i != len(fields)-1 {
			g.bitFieldPart[v] = struct{}{}
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
}

func (g *Generator) writePrefixedBitField(w printer, belong string, prefix string, prefix_size uint64, fields []*ast2go.Field) {
	t, ok := fields[1].FieldType.(*ast2go.StructUnionType)
	if !ok || t.Cond == nil || t.Cond.Expr.GetNodeType() != ast2go.NodeTypeIdent || !t.Exhaustive || len(t.UnionFields) != 1 {
		return
	}
	prefixIdent := t.Cond.Expr.(*ast2go.Ident)
	prefixIdent, _ = gen.LookupBase(prefixIdent)
	if prefixIdent != fields[0].Ident {
		return
	}
	unionField := t.UnionFields[0]
	unionType := unionField.FieldType.(*ast2go.UnionType)
	maxFieldSize := uint64(0)
	for _, cand := range unionType.Candidates {
		if cand.Field == nil {
			return
		}
		int_ty, ok := cand.Field.FieldType.(*ast2go.IntType)
		if !ok {
			return
		}
		if (prefix_size+*int_ty.BitSize)%8 != 0 {
			return
		}
		maxFieldSize = max(maxFieldSize, *int_ty.BitSize+prefix_size)
	}
	if maxFieldSize > 64 {
		return
	}
	seq := g.getSeq()
	w.Printf("flags%d uint%d\n", seq, maxFieldSize)

	// get/set prefix bit function
	prefixType := fmt.Sprintf("uint%d", gen.AlignInt(prefix_size))
	valueType := fmt.Sprintf("uint%d", maxFieldSize)
	compare := ""
	if prefix_size == 1 {
		prefixType = "bool"
		compare = " != 0"
	}
	g.PrintfFunc("func (t *%s) %s() %s {\n", belong, fields[0].Ident.Ident, prefixType)
	g.PrintfFunc("return %s((t.%sflags%d & 0x%x) >> %d %s)\n", prefixType, prefix, seq, ((uint64(1)<<prefix_size)-1)<<(maxFieldSize-prefix_size), maxFieldSize-prefix_size, compare)
	g.PrintfFunc("}\n")
	g.PrintfFunc("func (t *%s) Set%s(v %s) bool {\n", belong, fields[0].Ident.Ident, prefixType)
	value := "v"
	if prefixType != "bool" {
		g.PrintfFunc("if v > %d {\n", (1<<prefix_size)-1)
		g.PrintfFunc("return false\n")
		g.PrintfFunc("}\n")
	} else {
		g.PrintfFunc("var tmp uint%d\n", maxFieldSize)
		value = "tmp"
		g.PrintfFunc("if v {\n")
		g.PrintfFunc("tmp = 1\n")
		g.PrintfFunc("} else {\n")
		g.PrintfFunc("tmp = 0\n")
		g.PrintfFunc("}\n")
	}
	shift := maxFieldSize - prefix_size
	mask := ((uint64(1) << prefix_size) - 1) << shift
	g.PrintfFunc("t.%sflags%d = %s(t.%sflags%d & ^uint%d(0x%x)) | %s(%s) << %d\n", prefix, seq, valueType, prefix, seq, maxFieldSize, mask, valueType, value, shift)
	g.PrintfFunc("return true\n")
	g.PrintfFunc("}\n")
	if prefix_size == 1 {
		g.exprStringer.SetIdentMap(fields[0].Ident, fmt.Sprintf("(func() uint8 { if %%s%s() {return 1}else {return 0}}())", fields[0].Ident.Ident))
	} else {
		g.exprStringer.SetIdentMap(fields[0].Ident, fmt.Sprintf("%%s%s()", fields[0].Ident.Ident))
	}
	g.bitFieldPart[fields[0]] = struct{}{}
	// get/set variable bit function
	valueMask := (uint64(1) << (maxFieldSize - prefix_size)) - 1
	g.PrintfFunc("func (t *%s) %s() %s {\n", belong, unionField.Ident.Ident, valueType)
	g.PrintfFunc("return %s(t.%sflags%d & 0x%x)\n", valueType, prefix, seq, valueMask)
	g.PrintfFunc("}\n")
	g.PrintfFunc("func (t *%s) Set%s(v %s) bool {\n", belong, unionField.Ident.Ident, valueType)
	g.PrintfFunc("if v > %d {\n", (1<<maxFieldSize-prefix_size)-1)
	g.PrintfFunc("return false\n")
	g.PrintfFunc("}\n")
	g.PrintfFunc("t.%sflags%d = (t.%sflags%d & ^%s(0x%x)) | v\n", prefix, seq, prefix, seq, valueType, valueMask)
	g.PrintfFunc("return true\n")
	g.PrintfFunc("}\n")
	g.exprStringer.SetIdentMap(unionField.Ident, fmt.Sprintf("%%s%s()", unionField.Ident.Ident))
	g.prefixedBitField[t] = &PrefixedBitField{
		FieldName:   "flags" + strconv.Itoa(seq),
		Candidates:  unionType,
		PrefixSize:  prefix_size,
		MaxSize:     maxFieldSize,
		PrefixField: fields[0],
		UnionField:  unionField,
	}
}

func (g *Generator) writePrefixedBitFieldEncode(b *PrefixedBitField) {
	tmpVar := fmt.Sprintf("tmp%d", g.getSeq())
	cond := g.exprStringer.ExprString(b.Candidates.Cond)
	shouldElse := false
	for _, c := range b.Candidates.Candidates {
		condNum := g.exprStringer.ExprString(c.Cond)
		if shouldElse {
			g.PrintfFunc("else ")
		} else {
			shouldElse = true
		}
		g.PrintfFunc("if %s == %s {\n", cond, condNum)
		bitSize := *c.Field.FieldType.GetBitSize() + b.PrefixSize
		typ := fmt.Sprintf("uint%d", bitSize)
		g.PrintfFunc("var %s %s\n", tmpVar, typ)
		ident := g.exprStringer.ExprString(b.UnionField.Ident)
		g.PrintfFunc("%s = %s(%s)\n", tmpVar, typ, ident)
		g.PrintfFunc("%s |= %s(%s) << %d\n", tmpVar, typ, condNum, bitSize-b.PrefixSize)
		g.writeAppendUint(bitSize, tmpVar, ast2go.EndianUnspec)
		g.PrintfFunc("}")
	}
	g.PrintfFunc("\n")
}

func (g *Generator) writePrefixedBitFieldDecode(b *PrefixedBitField) {
	tmpVar := fmt.Sprintf("tmp%d", g.getSeq())
	tmpBytes := fmt.Sprintf("tmpBytes%d", g.getSeq())
	cond := g.exprStringer.ExprString(b.Candidates.Cond)
	shouldElse := false
	g.PrintfFunc("var %s [1]byte\n", tmpBytes)
	g.PrintfFunc("if _,err := io.ReadFull(r,%s[:]);err != nil {\n", tmpBytes)
	g.imports["fmt"] = struct{}{}
	g.PrintfFunc("return fmt.Errorf(\"failed to read prefix: %%w\",err)\n")
	g.PrintfFunc("}\n")
	oldReader := fmt.Sprintf("oldReader%d", g.getSeq())
	g.PrintfFunc("%s := r\n", oldReader)
	g.PrintfFunc("r = io.MultiReader(bytes.NewReader(%s[:]),r)\n", tmpBytes)
	prefixField := g.exprStringer.ExprString(b.PrefixField.Ident)
	toReplace := regexp.MustCompile(fmt.Sprintf(`%s\(\)`, b.PrefixField.Ident.Ident))
	compare := ""
	if b.PrefixSize == 1 {
		compare = " != 0"
	}
	setField := toReplace.ReplaceAllString(prefixField, fmt.Sprintf("Set%s(%s[0] >> %d%s)", b.PrefixField.Ident.Ident, tmpBytes, 8-b.PrefixSize, compare))
	g.PrintfFunc("%s\n", setField)
	for _, c := range b.Candidates.Candidates {
		condNum := g.exprStringer.ExprString(c.Cond)
		if shouldElse {
			g.PrintfFunc("else ")
		} else {
			shouldElse = true
		}
		g.PrintfFunc("if %s == %s {\n", cond, condNum)
		bitSize := *c.Field.FieldType.GetBitSize() + b.PrefixSize
		typ := fmt.Sprintf("uint%d", bitSize)
		g.PrintfFunc("var %s %s\n", tmpVar, typ)
		g.writeReadUint(bitSize, tmpVar, tmpVar, false, nil, ast2go.EndianUnspec)
		g.PrintfFunc("%s &= ^%s(1 << %d)\n", tmpVar, typ, bitSize-b.PrefixSize)
		setField := g.exprStringer.ExprString(b.UnionField.Ident)
		toReplace := regexp.MustCompile(fmt.Sprintf(`%s\(\)`, b.UnionField.Ident.Ident))
		maxType := fmt.Sprintf("uint%d", b.MaxSize)
		setField = toReplace.ReplaceAllString(setField, fmt.Sprintf("Set%s(%s(%s))", b.UnionField.Ident.Ident, maxType, tmpVar))
		g.PrintfFunc("%s\n", setField)
		g.PrintfFunc("}")
	}
	g.PrintfFunc("\nr = %s\n", oldReader)
}

func (g *Generator) writePrefixedBitFieldCode(b *PrefixedBitField, enc bool) {
	if enc {
		g.writePrefixedBitFieldEncode(b)
	} else {
		g.writePrefixedBitFieldDecode(b)
	}
}

func (g *Generator) writeBitField(w printer, belong string, prefix string, fields []*ast2go.Field /*size uint64, intSet, simple bool*/) {
	size, prefix_size := g.sumFields(fields)
	if size != nil {
		g.writeFullBitField(w, belong, prefix, *size, fields)
	} else if len(fields) == 2 && prefix_size == *fields[0].FieldType.GetBitSize() && prefix_size < 8 {
		g.writePrefixedBitField(w, belong, prefix, prefix_size, fields)
	}
}

func (g *Generator) InsertOverflowCheck(target string, compareType string, limitType string) {
	g.PrintfFunc("if %s > %s(^%s(0)) {\n", target, compareType, limitType)
	g.PrintfFunc("return false\n")
	g.PrintfFunc("}\n")
}

func (g *Generator) maybeWriteLengthSet(toSet string, length ast2go.Expr, setUnion func()) {
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
			setUnion()
			g.PrintfFunc("%s = %s(%s)\n", setTo, typ, toSet)
		} else {
			setUnion()
		}
	} else {
		setUnion()
	}
}

type printer interface {
	Printf(format string, args ...interface{})
}

type printerImpl struct {
	w io.Writer
}

func (p *printerImpl) Printf(format string, args ...interface{}) {
	fmt.Fprintf(p.w, format, args...)
}

func (g *Generator) writeStructUnion(w printer, belong string, prefix string, u *ast2go.StructUnionType, field *ast2go.Field) {
	union_field_ := fmt.Sprintf("union%d_", g.getSeq())
	union_type_ := fmt.Sprintf("union%d_%s", g.getSeq(), belong)
	g.Printf("type %s interface {\n", union_type_)
	g.Printf("is%s()\n", union_field_)
	g.Printf("}\n")
	for _, v := range u.Structs {
		seq := g.getSeq()
		s := fmt.Sprintf("union_%d_t", seq)
		nextIdent := fmt.Sprintf("%s%s.(*%s).", prefix, union_field_, s)
		nestw := &printerImpl{w: &bytes.Buffer{}}
		g.writeStructType(nestw, belong, nextIdent, v)
		g.Printf("type %s struct {", s)
		g.Printf("%s", nestw.w.(*bytes.Buffer).String())
		g.Printf("}\n")
		g.PrintfFunc("func (t *%s) is%s() {}\n", s, union_field_)
		mapIdent := &ast2go.Ident{Ident: prefix + union_field_, Base: field}
		g.unionStructs[v] = &UnionStruct{
			Struct:    v,
			TypeName:  s,
			FieldName: mapIdent,
		}
		g.exprStringer.SetIdentMap(mapIdent, "%s"+prefix+union_field_)
	}
	w.Printf("%s %s\n", union_field_, union_type_)
	for _, field := range u.UnionFields {
		typ := field.FieldType.(*ast2go.UnionType)
		typStr := "any"
		pointer := ""
		if typ.CommonType != nil {
			typStr = g.getType(typ.CommonType)
			pointer = "*"
		}
		// write getter func
		g.PrintfFunc("func (t *%s) %s() %s%s {\n", belong, field.Ident.Ident, pointer, typStr)
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
					checkUnion := func() {
						if s, ok := g.unionStructs[c.Field.BelongStruct]; ok {
							fieldName := g.exprStringer.ExprString(s.FieldName)
							g.PrintfFunc("if _,ok := %s.(*%s);!ok{\n", fieldName, s.TypeName)
							g.PrintfFunc("return nil // not set\n")
							g.PrintfFunc("}\n")
						}
					}
					ident := g.exprStringer.ExprString(c.Field.Ident)
					arr, ok := c.Field.FieldType.(*ast2go.ArrayType)
					if typStr == "any" {
						checkUnion()
						g.PrintfFunc("return &%s\n", ident)
					} else {
						checkUnion()
						if ok && arr.LengthValue != nil {
							g.PrintfFunc("tmp := %s(%s[:])\n", typStr, ident)
						} else {
							g.PrintfFunc("tmp := %s(%s)\n", typStr, ident)
						}
						g.PrintfFunc("return &tmp\n")
					}
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
					setUnion := func() {
						if s, ok := g.unionStructs[c.Field.BelongStruct]; ok {
							fieldName := g.exprStringer.ExprString(s.FieldName)
							g.PrintfFunc("if _,ok := %s.(*%s);!ok{\n", fieldName, s.TypeName)
							g.PrintfFunc("%s = &%s{}\n", fieldName, s.TypeName)
							g.PrintfFunc("}\n")
						}
					}
					if typStr == "any" {
						tmps := fmt.Sprintf("tmp%d", g.getSeq())
						g.PrintfFunc("%s, ok := v.(%s)\n", tmps, fieldType)
						g.PrintfFunc("if !ok {\n")
						g.PrintfFunc("return false\n")
						g.PrintfFunc("}\n")
						if arr, ok := c.Field.FieldType.(*ast2go.ArrayType); ok {
							g.maybeWriteLengthSet("len("+tmps+")", arr.Length, setUnion)
						} else {
							setUnion()
						}
						g.PrintfFunc("%s = %s\n", s, tmps)
					} else {
						if fieldType != typStr && c.Field.FieldType.GetNodeType() == ast2go.NodeTypeIntType {
							g.InsertOverflowCheck("v", typStr, fieldType)
							setUnion()
						} else if arr, ok := c.Field.FieldType.(*ast2go.ArrayType); ok {
							g.maybeWriteLengthSet("len(v)", arr.Length, setUnion)
						} else {
							setUnion()
						}
						g.PrintfFunc("%s = %s(v)\n", s, fieldType)
					}
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

func (g *Generator) writeStructType(w printer, belong string, prefix string, s *ast2go.StructType) {
	var non_aligned []*ast2go.Field
	/*
		var (
			is_int_set        = true
			is_simple         = true
			size       uint64 = 0
		)
	*/
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
				/*
					is_int_set = is_int_set && typ.GetNonDynamicAllocation()
					is_simple = is_simple &&
						(typ.GetNodeType() == ast2go.NodeTypeIntType ||
							typ.GetNodeType() == ast2go.NodeTypeEnumType)
					size += *typ.GetBitSize()
				*/
				continue
			}
			if len(non_aligned) > 0 {
				non_aligned = append(non_aligned, field)
				/*
					is_int_set = is_int_set && typ.GetNonDynamicAllocation()
					is_simple = is_simple &&
						(typ.GetNodeType() == ast2go.NodeTypeIntType ||
							typ.GetNodeType() == ast2go.NodeTypeEnumType)
					s := typ.GetBitSize()
					if s != nil {
						size += *s
					}
				*/
				g.writeBitField(w, belong, prefix, non_aligned /*size, is_int_set, is_simple*/)
				non_aligned = nil
				/*
					is_int_set = true
					is_simple = true
					size = 0
				*/
				continue
			}
			if u, ok := typ.(*ast2go.StructUnionType); ok {
				g.writeStructUnion(w, belong, prefix, u, field)
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
				w.Printf("%s %s\n", field.Ident.Ident, typ)
				g.exprStringer.SetIdentMap(field.Ident, "%s"+field.Ident.Ident)
				if gen.IsAnyRange(arr_type.Length) {
					if field.EventualFollow == ast2go.FollowEnd {
						g.laterSize[field] = field.BelongStruct.FixedTailSize
					} else if field.Next != nil && field.Next.FieldType.GetNodeType() == ast2go.NodeTypeStrLiteralType {
						g.terminalPattern[field] = struct{}{}
					}
				}
				if gen.IsOnNamedStruct(field) && arr_type.LengthValue == nil && !gen.IsAnyRange(arr_type.Length) {
					s := g.exprStringer.ExprString(field.Ident)
					g.PrintfFunc("func (t *%s) Set%s(v %s) bool {\n", belong, field.Ident.Ident, typ)
					g.maybeWriteLengthSet("len(v)", arr_type.Length, func() {})
					g.PrintfFunc("%s = v\n", s)
					g.PrintfFunc("return true\n")
					g.PrintfFunc("}\n")
				}
			}
			if i_type, ok := typ.(*ast2go.IntType); ok {
				typ := g.getType(i_type)
				w.Printf("%s %s\n", field.Ident.Ident, typ)
			}
			if f_typ, ok := typ.(*ast2go.FloatType); ok {
				typ := g.getType(f_typ)
				w.Printf("%s %s\n", field.Ident.Ident, typ)
			}
			if e_type, ok := typ.(*ast2go.EnumType); ok {
				typ := g.getType(e_type)
				w.Printf("%s %s\n", field.Ident.Ident, typ)
			}
			if s_type, ok := typ.(*ast2go.StrLiteralType); ok {
				magic := s_type.Base.Value
				w.Printf("// %s (%d byte)\n", magic, *typ.GetBitSize()/8)
				g.exprStringer.SetIdentMap(field.Ident, magic)
				continue
			}
			if u, ok := typ.(*ast2go.StructType); ok {
				typ := g.getType(u)
				w.Printf("%s %s\n", field.Ident.Ident, typ)
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
	if !*useVisitor {
		return
	}
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
		g.Printf("type %sVisitable interface {\n", g.visitorName)
		g.Printf("Visit(v %s)\n", g.visitorName)
		g.Printf("}\n")
		g.Printf("func %sToMap(v any) interface{} {", g.visitorName)
		{
			g.Printf("if v == nil {\n")
			g.Printf("return nil\n")
			g.Printf("}\n")
			g.Printf("if inter,ok := v.(%sVisitable);ok{\n", g.visitorName)
			g.Printf("if p := reflect.ValueOf(inter); p.Kind() == reflect.Pointer && p.IsNil() {\n")
			g.Printf("return nil\n")
			g.Printf("}\n")
			g.Printf("m := map[string]interface{}{}\n")
			g.Printf("inter.Visit(%sFunc(func(v %s,name string,field any){\n", g.visitorName, g.visitorName)
			g.Printf("m[name] = %sToMap(field)\n", g.visitorName)
			g.Printf("}))\n")
			g.Printf("return m\n")
			g.Printf("}\n")
			g.imports["reflect"] = struct{}{}
			// if array of non-primitive type, then call ToMap for each element
			g.Printf("if tf := reflect.TypeOf(v); (tf.Kind() == reflect.Slice || tf.Kind() == reflect.Array)&& ")
			g.Printf("!(func() bool { k := tf.Elem().Kind().String(); return k[:3] == \"int\" || k[:3] == \"uin\" || k[:3] == \"flo\" })() {\n")
			g.Printf("m := []interface{}{}\n")
			g.Printf("vf := reflect.ValueOf(v)\n")
			g.Printf("for i:=0;i<vf.Len();i++{\n")
			g.Printf("index := vf.Index(i)\n")
			g.Printf("if index.Kind() == reflect.Struct && index.CanAddr() {\n")
			g.Printf("index = index.Addr()\n")
			g.Printf("}\n")
			g.Printf("m = append(m,%sToMap(index.Interface()))\n", g.visitorName)
			g.Printf("}\n")
			g.Printf("return m\n")
			g.Printf("}\n")
			g.Printf("if tf := reflect.TypeOf(v); tf.Kind() == reflect.Pointer {\n")
			g.Printf("val := reflect.ValueOf(v)\n")
			g.Printf("if val.IsNil() {\n")
			g.Printf("return nil\n")
			g.Printf("}\n")
			g.Printf("return %sToMap(val.Elem().Interface())\n", g.visitorName)
			g.Printf("}\n")
			// otherwise, return the value itself
			g.Printf("return v\n")
		}
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
			if !strings.HasSuffix(expr, ")") && !strings.Contains(expr, `"`) {
				expr = "&" + expr
			}
			g.PrintfFunc("v.Visit(v,%q,%s)\n", field.Ident.Ident, expr)
		}
	}
	g.PrintfFunc("}\n")
	if *jsonMarshal {
		g.imports["encoding/json"] = struct{}{}
		g.PrintfFunc("func (t *%s) MarshalJSON() ([]byte, error) {\n", name)
		g.PrintfFunc("return json.Marshal(%sToMap(t))\n", g.visitorName)
		g.PrintfFunc("}\n")
	}
}

func (g *Generator) writeFormat(p *ast2go.Format) {
	g.StructNames = append(g.StructNames, p.Ident.Ident)
	w := &printerImpl{w: &bytes.Buffer{}}
	g.writeStructType(w, p.Ident.Ident, "", p.Body.StructType)
	g.Printf("type %s struct {\n", p.Ident.Ident)
	g.Printf("%s", w.w.(*bytes.Buffer).String())
	g.Printf("}\n")
	g.writeStructVisitor(p.Ident.Ident, p.Body.StructType)
	g.writeEncode(p)
	g.writeDecode(p)
}

func (g *Generator) writeAppendUint(size uint64, field string, endian ast2go.Endian) {
	if size == 8 {
		g.PrintfFunc("if n, err := w.Write([]byte{byte(%s)}); err != nil || n != 1 {\n", field)
		g.imports["fmt"] = struct{}{}
		g.PrintfFunc("return fmt.Errorf(\"encode %s: %%w\", err)\n", field)
		g.PrintfFunc("}\n")
		//g.PrintfFunc("buf = append(buf, byte(%s))\n", field)
		return
	}
	endian = g.decideEndian(endian)
	if size == 24 {
		if endian == ast2go.EndianBig {
			g.PrintfFunc("if n, err := w.Write([]byte{byte(%s>>16),byte(%s>>8),byte(%s)}); err != nil || n != 3 {\n", field, field, field)
		} else {
			g.PrintfFunc("if n, err := w.Write([]byte{byte(%s),byte(%s>>8),byte(%s>>16)}); err != nil || n != 3 {\n", field, field, field)
		}
		g.imports["fmt"] = struct{}{}
		g.PrintfFunc("return fmt.Errorf(\"encode %s: %%w\", err)\n", field)
		g.PrintfFunc("}\n")
		//g.PrintfFunc("buf = append(buf, byte(%s>>16),byte(%s>>8),byte(%s))\n", field, field, field)
		return
	}
	g.imports["encoding/binary"] = struct{}{}
	tmp := fmt.Sprintf("tmp%d", g.getSeq())
	g.PrintfFunc("%s := [%d]byte{}\n", tmp, size/8)
	if endian == ast2go.EndianBig {
		g.PrintfFunc("binary.BigEndian.PutUint%d(%s[:], uint%d(%s))\n", size, tmp, size, field)
	} else {
		g.PrintfFunc("binary.LittleEndian.PutUint%d(%s[:], uint%d(%s))\n", size, tmp, size, field)
	}
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
		g.writeAppendUint(*i_type.BitSize, ident, i_type.Endian)
		return
	}
	if f_typ, ok := typ.(*ast2go.FloatType); ok {
		tmpIdent := fmt.Sprintf("tmp%d", g.getSeq())
		if f_typ.IsCommonSupported {
			g.imports["math"] = struct{}{}
			g.PrintfFunc("%s := math.Float%dbits(%s)\n", tmpIdent, *f_typ.BitSize, ident)
		} else if *f_typ.BitSize == 16 {
			// TODO(on-keyday): fix this
			g.PrintfFunc("%s := uint16(math.Float32bits(float32(%s)))\n", tmpIdent, ident)
		}
		g.writeAppendUint(*f_typ.BitSize, tmpIdent, f_typ.Endian)
	}
	if enum_type, ok := typ.(*ast2go.EnumType); ok {
		g.writeAppendUint(*enum_type.BitSize, ident, enum_type.Base.BaseType.(*ast2go.IntType).Endian)
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
			if _, ok := g.terminalPattern[p]; ok {
				lit, _ := p.Next.FieldType.(*ast2go.StrLiteralType)
				g.imports["bytes"] = struct{}{}
				g.PrintfFunc("if bytes.Contains(%s,[]byte(%s)) {\n", ident, lit.Base.Value)
				g.imports["fmt"] = struct{}{}
				g.PrintfFunc("return fmt.Errorf(\"encode %s: contains terminal pattern %%q\", %s)\n", p.Ident.Ident, lit.Base.Value)
				g.PrintfFunc("}\n")
			} else if _, ok := g.laterSize[p]; !ok {
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
			return
		} else {
			if _, ok := g.terminalPattern[p]; ok {
				// nothing to do
			} else if _, ok := g.laterSize[p]; !ok {
				length := g.exprStringer.ExprString(arr_type.Length)
				g.PrintfFunc("len_%s := int(%s)\n", p.Ident.Ident, length)
				g.PrintfFunc("if len(%s) != len_%s {\n", ident, p.Ident.Ident)
				g.imports["fmt"] = struct{}{}
				g.PrintfFunc("return fmt.Errorf(\"encode %s: expect %%d but got %%d for length\", len_%s, len(%s))\n", p.Ident.Ident, p.Ident.Ident, ident)
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
		g.writeAppendUint(b.Size, converted, ast2go.EndianUnspec)
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

func (g *Generator) writeReadUint(size uint64, tmpName, field string, sign bool, enumTy *string, endian ast2go.Endian) {
	g.PrintfFunc("tmp%s := [%d]byte{}\n", tmpName, size/8)
	g.PrintfFunc("n_%s, err := io.ReadFull(r,tmp%s[:])\n", tmpName, tmpName)
	g.PrintfFunc("if err != nil {\n")
	g.imports["fmt"] = struct{}{}
	if size == 8 {
		g.PrintfFunc("return fmt.Errorf(\"read %s: expect 1 byte but read %%d bytes: %%w\", n_%s, err)\n", tmpName, tmpName)
	} else {
		g.PrintfFunc("return fmt.Errorf(\"read %s: expect %d bytes but read %%d bytes: %%w\", n_%s,err)\n", tmpName, size/8, tmpName)
	}
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
	endian = g.decideEndian(endian)
	if size == 8 {
		g.PrintfFunc("%s = %s(tmp%s[0])\n", field, castTo, tmpName)
	} else if endian == ast2go.EndianBig {
		if size == 24 {
			g.PrintfFunc("%s = %s(uint32(tmp%s[0])<<16 | uint32(tmp%s[1])<<8 | uint32(tmp%s[2]))\n", field, castTo, tmpName, tmpName, tmpName)
		} else {
			g.PrintfFunc("%s = %s(binary.BigEndian.Uint%d(tmp%s[:]))\n", field, castTo, size, tmpName)
		}
	} else {
		if size == 24 {
			g.PrintfFunc("%s = %s(uint32(tmp%s[2])<<16 | uint32(tmp%s[1])<<8 | uint32(tmp%s[0]))\n", field, castTo, tmpName, tmpName, tmpName)
		} else {
			g.PrintfFunc("%s = %s(binary.LittleEndian.Uint%d(tmp%s[:]))\n", field, castTo, size, tmpName)
		}
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
		g.writeReadUint(*i_type.BitSize, p.Ident.Ident, ident, i_type.IsSigned, nil, i_type.Endian)
		return
	}
	if f_type, ok := typ.(*ast2go.FloatType); ok {
		g.imports["math"] = struct{}{}
		tmpIdent := fmt.Sprintf("tmp_%s", p.Ident.Ident)
		g.PrintfFunc("var %s %s\n", tmpIdent, "uint"+strconv.FormatUint(*f_type.BitSize, 10))
		g.writeReadUint(*f_type.BitSize, p.Ident.Ident, tmpIdent, false, nil, f_type.Endian)
		if f_type.IsCommonSupported {
			g.PrintfFunc("%s = math.Float%dfrombits(uint%d(%s))\n", ident, *f_type.BitSize, *f_type.BitSize, tmpIdent)
		} else if *f_type.BitSize == 16 {
			// TODO(on-keyday): Fix this
			g.PrintfFunc("%s = float64(%s)\n", ident, tmpIdent)
		}
	}
	if enum_type, ok := typ.(*ast2go.EnumType); ok {
		g.writeReadUint(*enum_type.BitSize, p.Ident.Ident, ident, false, &enum_type.Base.Ident.Ident, enum_type.Base.BaseType.(*ast2go.IntType).Endian)
		return
	}
	if arr_type, ok := typ.(*ast2go.ArrayType); ok {
		length := g.exprStringer.ExprString(arr_type.Length)
		if i_typ, ok := arr_type.ElementType.(*ast2go.IntType); ok && *i_typ.BitSize == 8 {
			if arr_type.Length.GetConstantLevel() == ast2go.ConstantLevelConstant {
				g.PrintfFunc("n_%s, err := io.ReadFull(r,%s[:])\n", p.Ident.Ident, ident)
				g.PrintfFunc("if err != nil {\n")
				g.imports["fmt"] = struct{}{}
				g.PrintfFunc("return fmt.Errorf(\"read %s: expect %%d bytes but read %%d bytes: %%w\", %s, n_%s,err)\n", p.Ident.Ident, length, p.Ident.Ident)
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
			} else if _, ok := g.terminalPattern[p]; ok { // use terminal pattern
				next, _ := p.Next.FieldType.(*ast2go.StrLiteralType)
				g.imports["bufio"] = struct{}{}
				g.PrintfFunc("len_next_%s := len(%s)\n", p.Ident.Ident, next.Base.Value)
				g.PrintfFunc("peek_reader_%s := bufio.NewReaderSize(r,len_next_%s)\n", p.Ident.Ident, p.Ident.Ident)
				g.PrintfFunc("r = peek_reader_%s\n", p.Ident.Ident)
				g.PrintfFunc("buffer_%s := make([]byte,0)\n", p.Ident.Ident)
				g.PrintfFunc("for {\n")
				g.PrintfFunc("b, err := peek_reader_%s.Peek(len_next_%s)\n", p.Ident.Ident, p.Ident.Ident)
				g.PrintfFunc("if err != nil {\n")
				g.imports["fmt"] = struct{}{}
				g.PrintfFunc("return fmt.Errorf(\"read %s:peek next failed: %%w\", err)\n", p.Ident.Ident)
				g.PrintfFunc("}\n")
				g.PrintfFunc("if bytes.Equal(b, []byte(%s)) {\n", next.Base.Value)
				g.PrintfFunc("break\n")
				g.PrintfFunc("}\n")
				g.PrintfFunc("b_%s, err := peek_reader_%s.ReadByte()\n", p.Ident.Ident, p.Ident.Ident)
				g.PrintfFunc("if err != nil {\n")
				g.imports["fmt"] = struct{}{}
				g.PrintfFunc("return fmt.Errorf(\"read %s:read byte failed %%w\", err)\n", p.Ident.Ident)
				g.PrintfFunc("}\n")
				g.PrintfFunc("buffer_%s = append(buffer_%s, b_%s)\n", p.Ident.Ident, p.Ident.Ident, p.Ident.Ident)
				g.PrintfFunc("}\n")
				g.PrintfFunc("%s = buffer_%s\n", ident, p.Ident.Ident)
				return // not restore r so that caller can read next
			}
			g.PrintfFunc("len_%s := int(%s)\n", p.Ident.Ident, length)
			g.PrintfFunc("if len_%s != 0 {\n", p.Ident.Ident)
			g.PrintfFunc("tmp%s := make([]byte, len_%s)\n", p.Ident.Ident, p.Ident.Ident)
			g.PrintfFunc("n_%s, err := io.ReadFull(r,tmp%s[:])\n", p.Ident.Ident, p.Ident.Ident)
			g.PrintfFunc("if err != nil {\n")
			g.imports["fmt"] = struct{}{}
			g.PrintfFunc("return fmt.Errorf(\"read %s: expect %%d bytes but read %%d bytes: %%w\", len_%s, n_%s, err)\n", p.Ident.Ident, p.Ident.Ident, p.Ident.Ident)
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
					g.PrintfFunc("%s := bufio.NewReaderSize(r,1)\n", tmp)
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
				g.imports["fmt"] = struct{}{}
				g.PrintfFunc("return fmt.Errorf(\"read %s: expect %%d bytes but read %%d bytes: %%w\", len_%s, n_%s, err)\n", p.Ident.Ident, p.Ident.Ident, p.Ident.Ident)
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
			} else if _, ok := g.terminalPattern[p]; ok { // use terminal pattern
				next, _ := p.Next.FieldType.(*ast2go.StrLiteralType)
				g.imports["bufio"] = struct{}{}
				g.PrintfFunc("len_next_%s := len(%s)\n", p.Ident.Ident, next.Base.Value)
				g.PrintfFunc("peek_reader_%s := bufio.NewReaderSize(r,len_next_%s)\n", p.Ident.Ident, p.Ident.Ident)
				g.PrintfFunc("r = peek_reader_%s\n", p.Ident.Ident)
				g.PrintfFunc("for {\n")
				g.PrintfFunc("b, err := peek_reader_%s.Peek(len_next_%s)\n", p.Ident.Ident, p.Ident.Ident)
				g.PrintfFunc("if err != nil {\n")
				g.imports["fmt"] = struct{}{}
				g.PrintfFunc("return fmt.Errorf(\"read %s:peek next failed: %%w\", err)\n", p.Ident.Ident)
				g.PrintfFunc("}\n")
				g.PrintfFunc("if bytes.Equal(b, []byte(%s)) {\n", next.Base.Value)
				g.PrintfFunc("break\n")
				g.PrintfFunc("}\n")
				g.PrintfFunc("var tmp%s_ %s\n", p.Ident.Ident, g.getType(arr_type.ElementType))
				g.writeTypeDecode(fmt.Sprintf("tmp%s_", p.Ident.Ident), arr_type.ElementType, p)
				g.PrintfFunc("%s = append(%s, tmp%s_)\n", ident, ident, p.Ident.Ident)
				g.PrintfFunc("}\n")
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
		g.imports["fmt"] = struct{}{}
		g.PrintfFunc("return fmt.Errorf(\"read %s: expect %d bytes but read %%d bytes: %%w\", n_%s, err)\n", p.Ident.Ident, *t.GetBitSize()/8, tmp)
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
		g.writeReadUint(b.Size, b.Ident.Ident, "t."+b.Ident.Ident, false, nil, ast2go.EndianUnspec)
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
	if p := g.prefixedBitField[if_.StructUnionType]; p != nil {
		g.writePrefixedBitFieldCode(p, enc)
		return
	}
	g.PrintfFunc("if %s {\n", g.exprStringer.ExprString(if_.Cond))
	g.writeSingleNode(if_.Then, enc)
	for if_.Els != nil {
		if if_2, ok := if_.Els.(*ast2go.If); ok {
			g.PrintfFunc("} else if %s {\n", g.exprStringer.ExprString(if_2.Cond))
			g.writeSingleNode(if_2.Then, enc)
			if_ = if_2
		} else {
			g.PrintfFunc("} else {\n")
			g.writeSingleNode(if_.Els, enc)
			break
		}
	}
	g.PrintfFunc("}\n")
}

func (g *Generator) writeMatch(m *ast2go.Match, enc bool) {
	if p := g.prefixedBitField[m.StructUnionType]; p != nil {
		g.writePrefixedBitFieldCode(p, enc)
		return
	}
	g.PrintfFunc("switch {\n")
	cmp := &ast2go.Binary{
		Op:   ast2go.BinaryOpEqual,
		Left: m.Cond,
	}
	for _, mb := range m.Branch {
		if gen.IsAnyRange(mb.Cond) {
			g.PrintfFunc("default:\n")
		} else {
			cmp.Right = mb.Cond
			g.PrintfFunc("case %s:\n", g.exprStringer.ExprString(cmp))
		}

		g.writeSingleNode(mb.Then, enc)
	}
	g.PrintfFunc("}\n")
}

func (g *Generator) unionCheck(typ *ast2go.StructType, enc bool) {
	union_, ok := g.unionStructs[typ]
	if !ok {
		return // nothing to do
	}
	hasField := false
	for _, elem := range union_.Struct.Fields {
		if _, ok := elem.(*ast2go.Field); ok {
			hasField = true
			break
		}
	}
	if !hasField {
		return
	}
	fieldName := g.exprStringer.ExprString(union_.FieldName)
	if enc {
		g.PrintfFunc("if _,ok := %s.(*%s);!ok {\n", fieldName, union_.TypeName)
		g.imports["fmt"] = struct{}{}
		g.PrintfFunc("return fmt.Errorf(\"encode %s: union is not set to %s\")\n", fieldName, union_.TypeName)
		g.PrintfFunc("}\n")
	} else {
		g.PrintfFunc("%s = &%s{}\n", fieldName, union_.TypeName)
	}
}

func (g *Generator) writeSingleNode(node ast2go.Node, enc bool) {
	switch n := node.(type) {
	case *ast2go.IndentBlock:
		g.unionCheck(n.StructType, enc)
		for _, elem := range n.Elements {
			g.writeSingleNode(elem, enc)
		}
	case *ast2go.If:
		g.writeIf(n, enc)
	case *ast2go.Match:
		g.writeMatch(n, enc)
	case *ast2go.Field:
		if _, ok := n.Belong.(*ast2go.Function); ok {
			// TODO
		}
		if enc {
			g.writeFieldEncode(n)
		} else {
			g.writeFieldDecode(n)
		}
	case *ast2go.ScopedStatement:
		g.unionCheck(n.StructType, enc)
		g.writeSingleNode(n.Statement, enc)
	case *ast2go.Binary:
		if _, ok := g.globalAssigned[n]; ok {
			return // skip
		}
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

func (g *Generator) writeProgramElement(p *ast2go.Program) {
	for _, v := range p.Elements {

		if v, ok := v.(*ast2go.Enum); ok {
			g.writeEnum(v)
		}
		if v, ok := v.(*ast2go.Binary); ok {
			if v.Op == ast2go.BinaryOpConstAssign &&
				v.Right.GetConstantLevel() == ast2go.ConstantLevelConstant {
				ident := v.Left.(*ast2go.Ident)
				g.Printf("const %s = %s\n", ident.Ident, g.exprStringer.ExprString(v.Right))
				g.exprStringer.SetIdentMap(ident, "%s"+ident.Ident)
				g.globalAssigned[v] = struct{}{}
			} else if imports, ok := v.Right.(*ast2go.Import); ok {
				g.writeProgramElement(imports.ImportDesc)
			}
		}
	}
}

func (g *Generator) writeProgram(p *ast2go.Program) {
	conf := &gen.GenConfig{}
	conf.LookupGoConfig(p)
	if conf.PackageName == "" {
		conf.PackageName = "main"
	}
	g.writeProgramElement(p)
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
	for _, m := range p.Metadata {
		if m.Name == "config.word.map" {
			if len(m.Values) != 2 {
				continue
			}
			fromLit, ok := m.Values[0].(*ast2go.StrLiteral)
			if !ok {
				continue
			}
			toLit, ok := m.Values[1].(*ast2go.StrLiteral)
			if !ok {
				continue
			}
			from, err := strconv.Unquote(fromLit.Value)
			if err != nil {
				continue
			}
			to, err := strconv.Unquote(toLit.Value)
			if err != nil {
				continue
			}
			mappingWords[from] = to
		}
	}
	if p.Endian != nil && p.Endian.OrderValue != nil {
		if *p.Endian.OrderValue == 0 {
			g.defaultEndian = ast2go.EndianBig
		} else {
			g.defaultEndian = ast2go.EndianLittle
		}
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
