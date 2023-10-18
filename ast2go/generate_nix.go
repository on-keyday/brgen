//go:build !windows

//go:generate ../tool/gen_ast2go ast.go
//go:generate gofmt -w ast.go
package ast2go
