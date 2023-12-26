//go:build !js || !wasm

package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"os"

	ast2go "github.com/on-keyday/brgen/ast2go/ast"
)

func main() {
	flag.Parse()
	if *f {
		fmt.Println(`{
			"langs" : ["go"],
			"input" : "stdin",
			"suffix" : [".go"]
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
	g := NewGenerator()

	src, err := g.GenerateAndFormat(&file)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		if src == nil {
			os.Exit(1)
		}
	}

	os.Stdout.Write(src)
}
