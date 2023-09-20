package ast2go

import (
	"fmt"
	"go/ast"
	"go/constant"
	"go/token"
	"io"
	"strings"
)

type Generator struct {
	Config
	Output io.Writer
}

type Config struct {
	PackageName string
	ImportPath  []string
}

type ConfigDesc struct {
	Scope []string
}

func (c *ConfigDesc) String() string {
	return strings.Join(c.Scope, ".")
}

func GetConfig(m Node) string {
	if _, ok := m.(*ConfigNode); ok {
		return "config"
	}
	member, ok := m.(*MemberAccessNode)
	if !ok {
		return ""
	}
	desc := GetConfig(member.Target)
	if desc == "" {
		return ""
	}
	return desc + "." + member.Member
}

func mapToken(op string) token.Token {
	switch op {
	case "+":
		return token.ADD
	case "-":
		return token.SUB
	case "*":
		return token.MUL
	case "/":
		return token.QUO
	case "%":
		return token.REM
	case "=":
		return token.ASSIGN
	}
	return token.ILLEGAL
}

func ConvertAst(n Node) ast.Node {
	if n == nil {
		return nil
	}

	switch n := n.(type) {
	case *BinaryNode:
		return &ast.BinaryExpr{
			X:  ConvertAst(n.Left).(ast.Expr),
			Op: mapToken(n.Op),
			Y:  ConvertAst(n.Right).(ast.Expr),
		}
	case *BoolLiteralNode:
		if n.Value {
			return &ast.Ident{
				Name: "true",
			}
		} else {
			return &ast.Ident{
				Name: "false",
			}
		}
	case *IdentNode:
		return &ast.Ident{
			Name: n.Name,
		}
	case *StringLiteralNode:
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

func (g *Generator) lookupGoConfig(prog *Program) {
	for _, element := range prog.Elements {
		if c, ok := element.(*CallNode); ok {
			config := GetConfig(c.Callee)
			if config == "" {
				continue
			}
			if config == "config.go.import" {
				expr := ConvertAst(c.Args[0]).(ast.Expr)
				path := evalConstant(expr)
				g.Config.ImportPath = append(g.Config.ImportPath, path.String())
			}
		}
		if b, ok := element.(*BinaryNode); ok {
			if b.Op != "=" {
				continue
			}
			config := GetConfig(b.Left)
			if config == "" {
				continue
			}
			if config == "config.go.package" {
				expr := ConvertAst(b.Right).(ast.Expr)
				pkgName := evalConstant(expr)
				g.Config.PackageName = pkgName.String()
			}
		}
		if i, ok := element.(*ImportNode); ok {
			g.lookupGoConfig(i.ImportDesc)
		}
	}
}

func (g *Generator) printf(format string, v ...any) {
	fmt.Fprintf(g.Output, format, v...)
}

func NewGenerator(w io.Writer) *Generator {
	return &Generator{
		Output: w,
	}
}

func (g *Generator) Generate(file *File) (err error) {
	defer func() {
		if r := recover(); r != nil {
			err = fmt.Errorf("%v", r)
		}
	}()

	g.lookupGoConfig(file.Ast.Program)
	if g.Config.PackageName == "" {
		g.Config.PackageName = "main"
	}
	g.printf("package %s\n", g.Config.PackageName)
	if len(g.Config.ImportPath) > 0 {
		g.printf("import (\n")
		for _, path := range g.Config.ImportPath {
			g.printf("\t%q\n", path)
		}
		g.printf(")\n")
	}
	return nil
}
