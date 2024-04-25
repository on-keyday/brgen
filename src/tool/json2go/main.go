//go:build !js || !wasm

package main

import (
	"bytes"
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"os"

	ast2go "github.com/on-keyday/brgen/ast2go/ast"
	"github.com/on-keyday/brgen/ast2go/request"
)

func generate(r io.Reader, out func(data []byte, warn error), errOut func(err error)) {
	file := ast2go.AstFile{}
	err := json.NewDecoder(r).Decode(&file)
	if err != nil {
		errOut(err)
		return
	}
	g := NewGenerator()

	src, err := g.GenerateAndFormat(&file)
	if err != nil && src == nil {
		errOut(err)
	}
	if src != nil {
		out(src, err)
	}
}

func streamingMode() {
	stream, err := request.NewRequestStream(os.Stdin, os.Stdout)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	for {
		req, err := stream.Receive()
		if err != nil {
			break
		}
		r := bytes.NewReader(req.JsonText)
		go func() {
			generate(r, func(data []byte, warn error) {
				stream.RespondSource(req.ID, string(req.Name)+".go", data, err.Error())
			}, func(err error) {
				stream.RespondError(req.ID, err.Error())
			})
		}()
	}
}

var exitcode int

func main() {
	defer os.Exit(exitcode)
	flag.Parse()
	if *spec {
		if *filename {
			fmt.Println(`{
				"langs" : ["go"],
				"input" : "file",
				"suffix" : [".go"]
			}`)
		} else if *legacyStdin {
			fmt.Println(`{
			"langs" : ["go"],
			"input" : "stdin",
			"suffix" : [".go"]
		}`)
		} else {
			fmt.Println(`{
				"langs" : ["go"],
				"input" : "stdin_stream"
			}`)
		}
		return
	}

	if !*filename && !*legacyStdin {
		streamingMode()
		return
	}

	var f *os.File
	if *filename {
		if flag.NArg() < 1 {
			fmt.Fprintln(os.Stderr, "no filename")
			os.Exit(1)
			return
		}
		var err error
		f, err = os.Open(flag.Arg(0))
		if err != nil {
			fmt.Fprintln(os.Stderr, err)
			os.Exit(1)
			return
		}
		defer f.Close()
	} else {
		f = os.Stdin
	}

	generate(f, func(src []byte, warn error) {
		if warn != nil {
			fmt.Fprintln(os.Stderr, warn)
		}
		os.Stdout.Write(src)
	}, func(err error) {
		fmt.Fprintln(os.Stderr, err)
		exitcode = 1
	})
}
