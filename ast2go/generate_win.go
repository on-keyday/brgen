//go:build windows

//go:generate ../build/tool/gen_ast2go.exe ast.go
//go:generate gofmt -w ast.go
package ast2go
