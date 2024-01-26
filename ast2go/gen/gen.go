package gen

import (
	"fmt"
	"go/ast"
	"go/constant"
	"go/token"
	"io"
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

type ExprStringer struct {
	Receiver     string
	BinaryMapper map[ast2go.BinaryOp]func(s *ExprStringer, x, y ast2go.Expr) string
	IdentMapper  map[*ast2go.Ident]string
	CallMapper   map[string]func(s *ExprStringer, args ...ast2go.Expr) string
	TypeProvider func(ast2go.Type) string
}

func (s *ExprStringer) LookupBase(ident *ast2go.Ident) (*ast2go.Ident, bool) {
	viaMember := false
	for ident.Base != nil {
		if base, ok := ident.Base.(*ast2go.Ident); ok {
			ident = base
		} else if member, ok := ident.Base.(*ast2go.MemberAccess); ok {
			ident = member.Base
			viaMember = true
		} else {
			break
		}
	}
	return ident, viaMember
}

func (s *ExprStringer) SetIdentMap(ident *ast2go.Ident, name string) {
	b, _ := s.LookupBase(ident)
	if strings.Count(name, "%s") > 1 {
		panic("name must contains 1 '%s' for receiver")
	}
	s.IdentMapper[b] = name
}

func (s *ExprStringer) IdentToString(ident *ast2go.Ident, default_ string) string {
	b, viaMember := s.LookupBase(ident)
	mappedValue := s.IdentMapper[b]
	if mappedValue == "" {
		mappedValue = "%s"
	}
	if !strings.Contains(mappedValue, "%s") {
		return mappedValue
	}
	if !viaMember {
		if _, ok := b.Base.(ast2go.Member); ok {
			return fmt.Sprintf(mappedValue, s.Receiver)
		}
	}
	// special case for enum member
	// enum member is not a member access
	if _, ok := b.Base.(*ast2go.EnumMember); ok {
		return fmt.Sprintf(mappedValue, "")
	}
	return fmt.Sprintf(mappedValue, default_)
}

func NewExprStringer() *ExprStringer {
	return &ExprStringer{
		BinaryMapper: map[ast2go.BinaryOp]func(s *ExprStringer, x, y ast2go.Expr) string{},
		IdentMapper:  map[*ast2go.Ident]string{},
		CallMapper:   map[string]func(s *ExprStringer, args ...ast2go.Expr) string{},
	}
}

func (s *ExprStringer) ExprString(e ast2go.Expr) string {

	switch e := e.(type) {
	case *ast2go.Binary:
		if f, ok := s.BinaryMapper[e.Op]; ok {
			return f(s, e.Left, e.Right)
		}
		return fmt.Sprintf("(%s %s %s)", s.ExprString(e.Left), e.Op.String(), s.ExprString(e.Right))
	case *ast2go.Ident:
		return s.IdentToString(e, "")
	case *ast2go.IntLiteral:
		return e.Value
	case *ast2go.BoolLiteral:
		if e.Value {
			return "true"
		} else {
			return "false"
		}
	case *ast2go.MemberAccess:
		target := s.ExprString(e.Target)
		return s.IdentToString(e.Member, target+".")
	case *ast2go.Call:
		callee := s.ExprString(e.Callee)
		if f, ok := s.CallMapper[callee]; ok {
			return f(s, e.Arguments...)
		}
		args := []string{}
		for _, arg := range e.Arguments {
			args = append(args, s.ExprString(arg))
		}
		return fmt.Sprintf("%s(%s)", callee, strings.Join(args, ","))
	case *ast2go.Cond:
		if s.TypeProvider == nil {
			s.TypeProvider = s.GetType
		}
		typ := s.TypeProvider(e.ExprType)
		return fmt.Sprintf("(func() %s {if %s { return %s(%s) } else { return %s(%s) }}())", typ, s.ExprString(e.Cond), typ, s.ExprString(e.Then), typ, s.ExprString(e.Els))
	case *ast2go.Cast:
		if s.TypeProvider == nil {
			s.TypeProvider = s.GetType
		}
		typ := s.TypeProvider(e.ExprType)
		return fmt.Sprintf("%s(%s)", typ, s.ExprString(e.Expr))
	case *ast2go.Available:
		ident, ok := e.Target.(*ast2go.Ident)
		if !ok {
			return "false"
		}
		ident, ok = ident.Base.(*ast2go.Ident)
		if !ok {
			return "false"
		}
		field, ok := ident.Base.(*ast2go.Field)
		if !ok {
			return "false"
		}
		uty, ok := field.FieldType.(*ast2go.UnionType)
		if !ok {
			return "true" //always true
		}
		b := strings.Builder{}
		b.WriteString("(func() bool {")
		var cond0 ast2go.Expr
		if uty.Cond != nil {
			cond0 = uty.Cond
		} else {
			cond0 = &ast2go.BoolLiteral{
				ExprType: &ast2go.BoolType{},
				Value:    true,
			}
		}
		hasElse := false
		for _, c := range uty.Candidates {
			if c.Cond != nil {
				eq := &ast2go.Binary{
					Op:    ast2go.BinaryOpEqual,
					Left:  cond0,
					Right: c.Cond,
				}
				cond := s.ExprString(eq)
				if hasElse {
					b.WriteString("else ")
				}
				b.WriteString(fmt.Sprintf("if %s { ", cond))
				if c.Field != nil {
					b.WriteString("return true")
				} else {
					b.WriteString("return false")
				}
				b.WriteString(" }")
				hasElse = true
			} else {
				b.WriteString(" else { ")
				if c.Field != nil {
					b.WriteString("return true")
				} else {
					b.WriteString("return false")
				}
				b.WriteString(" }")
			}
		}
		b.WriteString("; return false")
		b.WriteString(" }())")
		return b.String()
	default:
		return ""
	}
}

func (s *ExprStringer) CollectDefine(e ast2go.Expr) []*ast2go.Binary {
	defines := []*ast2go.Binary{}
	ast2go.Walk(e, ast2go.VisitFn(func(v ast2go.Visitor, n ast2go.Node) bool {
		if i, ok := n.(*ast2go.Ident); ok {
			b := i.Base
			if b == nil {
				return true
			}
			bi, ok := i.Base.(*ast2go.Ident)
			if !ok {
				return true
			}
			if b, ok := bi.Base.(*ast2go.Binary); ok {
				if b.Op == ast2go.BinaryOpDefineAssign ||
					b.Op == ast2go.BinaryOpConstAssign {
					defines = append(defines, b)
				}
			}
		}
		return true
	}))
	return defines
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

func (g *ExprStringer) GetType(typ ast2go.Type) string {
	if g.TypeProvider == nil {
		g.TypeProvider = g.GetType
	}
	if ident_typ, ok := typ.(*ast2go.IdentType); ok {
		return ident_typ.Ident.Ident
	}
	if i_type, ok := typ.(*ast2go.IntType); ok {
		if i_type.IsSigned {
			return fmt.Sprintf("int%d", *i_type.BitSize)
		} else {
			return fmt.Sprintf("uint%d", *i_type.BitSize)
		}
	}
	if f_typ, ok := typ.(*ast2go.FloatType); ok {
		return fmt.Sprintf("float%d", *f_typ.BitSize)
	}
	if e_type, ok := typ.(*ast2go.EnumType); ok {
		return fmt.Sprintf("%s", e_type.Base.Ident.Ident)
	}
	if arr_type, ok := typ.(*ast2go.ArrayType); ok {
		if arr_type.Length != nil && arr_type.Length.GetConstantLevel() == ast2go.ConstantLevelConstant {
			len := g.ExprString(arr_type.Length)
			return fmt.Sprintf("[%s]%s", len, g.TypeProvider(arr_type.BaseType))
		}
		return fmt.Sprintf("[]%s", g.TypeProvider(arr_type.BaseType))
	}
	if struct_type, ok := typ.(*ast2go.StructType); ok {
		if !struct_type.Recursive {
			return fmt.Sprintf("%s", struct_type.Base.(*ast2go.Format).Ident.Ident)
		}
	}
	return ""
}
