package main

import (
	"errors"
	"io"

	"github.com/on-keyday/brgen/ast2go/ast"
	"github.com/on-keyday/brgen/ast2go/gen"
)

type Type interface {
	String() string
}

type Reference string

func (r Reference) String() string {
	return string(r)
}

type Case map[string]Type

type SwitchType struct {
	SwitchOn string `json:"switch-on"`
	Cases    Case   `json:"cases"`
}

type Field struct {
	ID       string `json:"id"`
	Type     Type   `json:"type,omitempty"`
	Repeat   string `json:"repeat,omitempty"`
	Enum     string `json:"enum,omitempty"`
	Size     string `json:"size,omitempty"`
	Contents string `json:"contents,omitempty"`
}

type Meta struct {
	ID string `json:"id"`
}

type Kaitai struct {
	Types map[string]Type `json:"types"`
}

func genKaitai(s *ast.Format, w io.Writer) {
	members := gen.CollectMember(s.Body.Elements)
	for _, member := range members {
		switch member.(type) {
		case *ast.Field:

		}
	}
}

func renderKaitai(root *ast.Program, w io.Writer) {
	var rootTypes []ast.Member
	gen.ConfigFromProgram(root, func(configName string, asCall bool, args ...ast.Expr) error {
		if configName == "config.export" {
			if !asCall {
				return errors.New("config.export must be called")
			}
			for _, arg := range args {
				ident, ok := arg.(*ast.Ident)
				if !ok {
					return errors.New("config.export must have identifier as argument")
				}
				typ, ok := ident.Base.(ast.Member)
				if !ok {
					return errors.New("config.export must have type as argument")
				}
				rootTypes = append(rootTypes, typ)
			}
		}
		return nil
	})
	for _, typ := range rootTypes {
		switch a := typ.(type) {
		case *ast.Format:
			genKaitai(a, w)
		}
	}
}
