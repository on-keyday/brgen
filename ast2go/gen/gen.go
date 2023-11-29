package gen

import (
	"fmt"
	"go/ast"
	"go/constant"
	"go/format"
	"go/token"
	"io"
	"runtime/debug"
	"strings"

	ast2go "github.com/on-keyday/brgen/ast2go/ast"
)

type Generator struct {
	Config GenConfig
	Output io.Writer
}

type GenConfig struct {
	PackageName string
	ImportPath  []string
}

func ConfigFromProgram(p *ast2go.Program, f func(configName string, asCall bool, args ...ast2go.Expr) error) error {
	for _, element := range p.Elements {
		if c, ok := element.(*ast2go.Call); ok {
			config := GetConfig(c.Callee)
			if config == "" {
				continue
			}
			if err := f(config, true, c.Arguments...); err != nil {
				return err
			}
		}
		if b, ok := element.(*ast2go.Binary); ok {
			if b.Op != ast2go.BinaryOpAssign {
				continue
			}
			config := GetConfig(b.Left)
			if config == "" {
				continue
			}
			if err := f(config, false, b.Right); err != nil {
				return err
			}
		}
		if i, ok := element.(*ast2go.Import); ok {
			ConfigFromProgram(i.ImportDesc, f)
		}
	}
	return nil
}

func CollectMember(elements []ast2go.Node) []ast2go.Member {
	var fields []ast2go.Member
	for _, element := range elements {
		if m, ok := element.(ast2go.Member); ok {
			fields = append(fields, m)
		}
	}
	return fields
}

type ConfigDesc struct {
	Scope []string
}

func (c *ConfigDesc) String() string {
	return strings.Join(c.Scope, ".")
}

func GetConfig(m ast2go.Node) string {
	if _, ok := m.(*ast2go.Config); ok {
		return "config"
	}
	member, ok := m.(*ast2go.MemberAccess)
	if !ok {
		return ""
	}
	desc := GetConfig(member.Target)
	if desc == "" {
		return ""
	}
	return desc + "." + member.Member.Ident
}

func mapToken(op ast2go.BinaryOp) token.Token {
	switch op {
	case ast2go.BinaryOpAdd:
		return token.ADD
	case ast2go.BinaryOpSub:
		return token.SUB
	case ast2go.BinaryOpMul:
		return token.MUL
	case ast2go.BinaryOpDiv:
		return token.QUO
	case ast2go.BinaryOpMod:
		return token.REM
	case ast2go.BinaryOpAssign:
		return token.ASSIGN
	}
	return token.ILLEGAL
}

func ConvertAst(n ast2go.Node) ast.Node {
	if n == nil {
		return nil
	}

	switch n := n.(type) {
	case *ast2go.Binary:
		return &ast.BinaryExpr{
			X:  ConvertAst(n.Left).(ast.Expr),
			Op: mapToken(n.Op),
			Y:  ConvertAst(n.Right).(ast.Expr),
		}
	case *ast2go.BoolLiteral:
		if n.Value {
			return &ast.Ident{
				Name: "true",
			}
		} else {
			return &ast.Ident{
				Name: "false",
			}
		}
	case *ast2go.Ident:
		return &ast.Ident{
			Name: n.Ident,
		}
	case *ast2go.StrLiteral:
		return &ast.BasicLit{
			Kind:  token.STRING,
			Value: n.Value,
		}
	}
	return nil
}

func evalConstant(e ast.Expr) constant.Value {
	switch e := e.(type) {
	case *ast.BasicLit:
		return constant.MakeFromLiteral(e.Value, e.Kind, 0)
	case *ast.BinaryExpr:
		x := evalConstant(e.X)
		y := evalConstant(e.Y)
		return constant.BinaryOp(x, e.Op, y)
	}
	return constant.MakeUnknown()
}

func (config *GenConfig) LookupGoConfig(prog *ast2go.Program) error {
	return ConfigFromProgram(prog, func(configName string, asCall bool, args ...ast2go.Expr) error {
		if configName == "config.go.import" {
			if !asCall {
				return fmt.Errorf("config.go.import must be call")
			}
			path := evalConstant(ConvertAst(args[0]).(ast.Expr))
			config.ImportPath = append(config.ImportPath, path.String())
		}
		if configName == "config.go.package" {
			if asCall {
				return fmt.Errorf("config.go.package must be assignment")
			}
			expr := ConvertAst(args[0]).(ast.Expr)
			pkgName := evalConstant(expr)
			config.PackageName = pkgName.String()
		}
		return nil
	})
}

func (g *Generator) printf(format string, v ...any) {
	fmt.Fprintf(g.Output, format, v...)
}

func NewGenerator(w io.Writer) *Generator {
	return &Generator{
		Output: w,
	}
}

type GoField interface {
	FieldName() string
	Type() string
	Encoder
	Decoder
}

type IntField struct {
	Name     string
	BitSize  uint64
	IsSigned bool
}

func (i *IntField) FieldName() string {
	return i.Name
}

func (i *IntField) Type() string {
	switch i.BitSize {
	case 8:
		if i.IsSigned {
			return "int8"
		} else {
			return "uint8"
		}
	case 16:
		if i.IsSigned {
			return "int16"
		} else {
			return "uint16"
		}
	case 32:
		if i.IsSigned {
			return "int32"
		} else {
			return "uint32"
		}
	case 64:
		if i.IsSigned {
			return "int64"
		} else {
			return "uint64"
		}
	}
	return ""
}

func (i *IntField) Encode() string {
	return fmt.Sprintf("buf.Write%s(%s)", i.Type(), i.Name)
}

func (i *IntField) Decode() string {
	return fmt.Sprintf("binary.Read(buf,%s,&%s)", i.Type(), i.Name)
}

type BitField struct {
	Name    string
	BitSize uint64
}

type BitFlagsField struct {
	TotalBitSize uint64
	Fields       []*BitField
}

func (b *BitFlagsField) FieldName() string {
	name := ""
	for _, field := range b.Fields {
		name += field.Name
	}
	return name
}

func (b *BitFlagsField) Type() string {
	return (&IntField{
		BitSize:  b.TotalBitSize,
		IsSigned: false,
	}).Type()
}

func (b *BitFlagsField) Encode() string {
	return fmt.Sprintf("buf.Write%s(%s)", b.Type(), b.FieldName())
}

func (b *BitFlagsField) Decode() string {
	return fmt.Sprintf("binary.Read(buf,%s,&%s)", b.Type(), b.FieldName())
}

type BytesField struct {
	Name string
	Size ast2go.Expr
}

func (b *BytesField) FieldName() string {
	return b.Name
}

func (b *BytesField) Type() string {
	return "[]byte"
}

func (b *BytesField) Encode() string {
	return fmt.Sprintf("buf.Write(%s)", b.Name)
}

func (b *BytesField) Decode() string {
	return fmt.Sprintf("binary.Read(buf,%s,&%s)", b.Type(), b.Name)
}

type ArrayField struct {
	Name   string
	Length ast2go.Expr
	Elem   GoField
}

func (a *ArrayField) FieldName() string {
	return a.Name
}

func (a *ArrayField) Type() string {
	return "[]" + a.Elem.Type()
}

func (a *ArrayField) Encode() string {
	return fmt.Sprintf("buf.Write(%s)", a.Name)
}

func (a *ArrayField) Decode() string {
	return fmt.Sprintf("binary.Read(buf,%s,&%s)", a.Type(), a.Name)
}

func isBuiltinBitSize(bitSize uint64) bool {
	return bitSize < 64 && bitSize&(bitSize-1) == 0 && bitSize%8 == 0
}

func getGoField(g ast2go.Type) GoField {
	switch t := g.(type) {
	case *ast2go.IntType:
		return &IntField{
			BitSize:  t.BitSize,
			IsSigned: t.IsSigned,
		}
	case *ast2go.ArrayType:
		if int_type, ok := t.BaseType.(*ast2go.IntType); ok && int_type.BitSize == 8 {
			return &BytesField{
				Size: t.Length,
			}
		} else {
			return &ArrayField{
				Length: t.Length,
				Elem:   getGoField(t.BaseType),
			}
		}
	}
	return nil
}

type Encoder interface {
	Encode() string
}

type Decoder interface {
	Decode() string
}

type GoStruct struct {
	Name     string
	Src      *ast2go.Format
	Body     []GoField
	fieldMap map[*ast2go.Field]GoField
	Encode   []Encoder
	Decode   []Decoder
}

func (g *Generator) collectFormat(f *ast2go.Format) (*GoStruct, error) {
	fieldList := []*ast2go.Field{}
	for _, member := range f.Body.Elements {
		if field, ok := member.(*ast2go.Field); ok {
			if field.Ident == nil {
				continue
			}
			fieldList = append(fieldList, field)
		}
	}
	goFields := []GoField{}
	var curBitType *BitFlagsField
	fieldMap := map[*ast2go.Field]GoField{}
	for _, field := range fieldList {
		switch ty := field.FieldType.(type) {
		case *ast2go.IntType:
			if curBitType != nil {
				if isBuiltinBitSize(ty.BitSize) {
					goFields = append(goFields, &IntField{
						BitSize:  ty.BitSize,
						IsSigned: ty.IsSigned,
					})
					fieldMap[field] = goFields[len(goFields)-1]
				} else {
					curBitType = &BitFlagsField{
						TotalBitSize: ty.BitSize,
					}
					curBitType.Fields = append(curBitType.Fields, &BitField{
						Name:    field.Ident.Ident,
						BitSize: ty.BitSize,
					})
					fieldMap[field] = curBitType
				}
			} else {
				curBitType.Fields = append(curBitType.Fields, &BitField{
					Name:    field.Ident.Ident,
					BitSize: ty.BitSize,
				})
				curBitType.TotalBitSize += ty.BitSize
				fieldMap[field] = curBitType
				if isBuiltinBitSize(curBitType.TotalBitSize) {
					goFields = append(goFields, curBitType)
					curBitType = nil
				} else if curBitType.TotalBitSize > 64 {
					return nil, fmt.Errorf("bit field size too large %v", curBitType.TotalBitSize)
				}

			}
		case *ast2go.ArrayType:
			if int_type := ty.BaseType.(*ast2go.IntType); int_type != nil && int_type.BitSize == 8 {
				goFields = append(goFields, &BytesField{
					Name: field.Ident.Ident,
					Size: ty.Length,
				})
				fieldMap[field] = goFields[len(goFields)-1]
			} else {
				goFields = append(goFields, &ArrayField{
					Name:   field.Ident.Ident,
					Length: ty.Length,
					Elem:   getGoField(ty.BaseType),
				})
				fieldMap[field] = goFields[len(goFields)-1]
			}
		}
	}
	if curBitType != nil {
		if !isBuiltinBitSize(curBitType.TotalBitSize) {
			return nil, fmt.Errorf("bit field size too large %v", curBitType.TotalBitSize)
		} else {
			goFields = append(goFields, curBitType)
		}
	}

	return &GoStruct{
		Name:     f.Ident.Ident,
		Src:      f,
		fieldMap: fieldMap,
		Body:     goFields,
	}, nil
}

type Coder interface {
	Encoder
	Decoder
}

type Comment struct {
	Comment string
}

func (c *Comment) Encode() string {
	return "//" + c.Comment
}

func (c *Comment) Decode() string {
	return "//" + c.Comment
}

type MultiLineComment struct {
	Comment string
}

func (m *MultiLineComment) Encode() string {
	return "/*" + m.Comment + "*/"
}

func (m *MultiLineComment) Decode() string {
	return "/*" + m.Comment + "*/"
}

type Elif struct {
	Cond ast2go.Expr
	Body []Coder
}

type IfCond struct {
	Cond ast2go.Expr
	Body []Coder
	Elif []Elif
	Els  []Coder
}

func (i *IfCond) Encode() string {
	b := &strings.Builder{}
	b.WriteString("if ")
	format.Node(b, token.NewFileSet(), ConvertAst(i.Cond))
	b.WriteString("{\n")
	for _, elm := range i.Body {
		b.WriteString(elm.Encode())
		b.WriteString("\n")
	}
	b.WriteString("}\n")
	return b.String()
}

func (i *IfCond) Decode() string {
	b := &strings.Builder{}
	b.WriteString("if ")
	format.Node(b, token.NewFileSet(), ConvertAst(i.Cond))
	b.WriteString("{\n")
	for _, elm := range i.Body {
		b.WriteString(elm.Decode())
		b.WriteString("\n")
	}
	b.WriteString("}\n")
	return b.String()
}

func (g *Generator) generateElementCoder(f *GoStruct, elms []ast2go.Node) []Coder {
	coders := []Coder{}
	handledFields := map[GoField]struct{}{}
	for _, elm := range elms {
		if comment := elm.(*ast2go.Comment); comment != nil {
			coders = append(coders, &Comment{
				Comment: comment.Comment,
			})
		}
		if comment := elm.(*ast2go.CommentGroup); comment != nil {
			for _, comment := range comment.Comments {
				coders = append(coders, &Comment{
					Comment: comment.Comment,
				})
			}
		}
		if cond := elm.(*ast2go.If); cond != nil {
			c := &IfCond{
				Cond: cond.Cond,
			}
			c.Body = g.generateElementCoder(f, cond.Then.Elements)
			for {
				if cond.Els == nil {
					break
				}
				if next, ok := cond.Els.(*ast2go.If); ok {
					c.Elif = append(c.Elif, Elif{
						Cond: next.Cond,
					})
					c.Body = g.generateElementCoder(f, next.Then.Elements)
					cond = next
				} else {
					block := cond.Els.(*ast2go.IndentBlock)
					c.Els = g.generateElementCoder(f, block.Elements)
					break
				}
			}
			coders = append(coders, c)
		}
		if a := elm.(*ast2go.Assert); a != nil {

		}
		if field, ok := elm.(*ast2go.Field); ok {
			if goField, ok := f.fieldMap[field]; ok {
				if _, ok := handledFields[goField]; ok {
					continue
				}
				handledFields[goField] = struct{}{}
				coders = append(coders, goField)
			}
		}
	}
	return coders
}

func (g *Generator) generateEncode(f *GoStruct) {

}

func (g *Generator) Generate(file *ast2go.AstFile) (err error) {
	defer func() {
		if r := recover(); r != nil {
			err = fmt.Errorf("%v: %v", r, string(debug.Stack()))
		}
	}()
	p, err := ast2go.ParseAST(file.Ast)
	if err != nil {
		return err
	}
	g.Config.LookupGoConfig(p)
	ConfigFromProgram(p, func(configName string, asCall bool, args ...ast2go.Expr) error {
		switch configName {
		case "config.go.package":
			if asCall {
				return fmt.Errorf("config.go.package must be assignment")
			}
			if len(args) != 1 {
				return fmt.Errorf("config.go.package must have 1 argument")
			}
			pkgName := evalConstant(ConvertAst(args[0]).(ast.Expr))
			g.Config.PackageName = pkgName.String()
		case "config.go.import":
			if !asCall {
				return fmt.Errorf("config.go.import must be call")
			}
			if len(args) != 1 {
				return fmt.Errorf("config.go.import must have 1 argument")
			}
			path := evalConstant(ConvertAst(args[0]).(ast.Expr))
			g.Config.ImportPath = append(g.Config.ImportPath, path.String())
		}
		return nil
	})
	if g.Config.PackageName == "" {
		g.Config.PackageName = "main"
	}
	g.printf("// Code generated by brgen. DO NOT EDIT.\n")
	g.printf("package %s\n", g.Config.PackageName)
	if len(g.Config.ImportPath) > 0 {
		g.printf("import (\n")
		for _, path := range g.Config.ImportPath {
			g.printf("\t%v\n", path)
		}
		g.printf(")\n")
	}
	g.printf("\n")
	fmts := []*GoStruct{}
	for _, element := range p.Elements {
		if f, ok := element.(*ast2go.Format); ok {
			fmt, err := g.collectFormat(f)
			if err != nil {
				return err
			}
			fmts = append(fmts, fmt)
		}
	}
	return nil
}
