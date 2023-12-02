//go:build !js || !wasm

package main

import (
	"bytes"
	"encoding/json"
	"flag"
	"fmt"
	"go/format"
	"os"

	ast2go "github.com/on-keyday/brgen/ast2go/ast"
)

func main() {
	flag.Parse()
	if *f {
		fmt.Println(`{
			"langs" : ["go"],
			"pass_by" : "stdin",
			"suffix" : ".go"
		}`)
		return
	}
	file := ast2go.AstFile{}
	if *filename != "" {
		f, err := os.Open(*filename)
		if err != nil {
			fmt.Fprintln(os.Stderr, err)
			os.Exit(1)
			return
		}
		defer f.Close()
		err = json.NewDecoder(f).Decode(&file)
		if err != nil {
			fmt.Fprintln(os.Stderr, err)
			os.Exit(1)
			return
		}
	} else {
		err := json.NewDecoder(os.Stdin).Decode(&file)
		if err != nil {
			fmt.Fprintln(os.Stderr, err)
			os.Exit(1)
			return
		}
	}
	buf := bytes.NewBuffer(nil)
	g := NewGenerator(buf)

	err := g.Generate(&file)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
		return
	}

	src, err := format.Source(buf.Bytes())
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		src = buf.Bytes() // anyway dump
	}
	os.Stdout.Write(src)
}
