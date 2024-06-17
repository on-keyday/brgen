//go:build windows

//go:generate ../tool/gen_ast2go.exe ast/ast.go
//go:generate gofmt -w ast/ast.go
package ast2go
