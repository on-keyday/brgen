//go:build js && wasm

package main

import (
	js "github.com/on-keyday/brgen/ast2go/js"
)

type makeGenerator struct{}

func (m *makeGenerator) New() js.Generator {
	return &Generator{}
}

func (m *makeGenerator) ResetFlag() {

}

func setFunc() {
	js.SetGoFunc(&makeGenerator{})
}

func main() {
	c := make(chan struct{})
	setFunc()
	<-c
}
