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

type LineMap struct {
	Line uint64     `json:"line"`
	Loc  ast2go.Loc `json:"loc"`
}

type TestInfo struct {
	LineMap []LineMap `json:"line_map"`
	Structs []string `json:"structs"`
}

func generate(r io.Reader, out func(suffix string, data []byte, warn error), errOut func(err error)) {
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
		out(".go", src, err)
		if *testInfo {
			var info TestInfo
			info.Structs = g.StructNames
			info.LineMap = []LineMap{}
			data, err := json.Marshal(info)
			out(".go.json", data, err)
		}
	}
}

func streamingMode() {
	handler := func(stream *request.IDStream, req *request.GenerateSource) {
		r := bytes.NewReader(req.JsonText)
		generate(r, func(suffix string, data []byte, warn error) {
			err := ""
			if warn != nil {
				err = warn.Error()
			}
			stream.RespondSource(string(req.Name)+suffix, data, err)
		}, func(err error) {
			stream.RespondError(err.Error())
		})
	}
	err := request.Run(os.Stdin, os.Stdout, handler)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
		return
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

	generate(f, func(suffix string, src []byte, warn error) {
		if warn != nil {
			fmt.Fprintln(os.Stderr, warn)
		}
		os.Stdout.Write(src)
	}, func(err error) {
		fmt.Fprintln(os.Stderr, err)
		exitcode = 1
	})
}
