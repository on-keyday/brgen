package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"os"

	"github.com/on-keyday/brgen/ast2go"
)

var f = flag.Bool("s", false, "tell spec of json2go")

func main() {
	flag.Parse()
	if *f {
		fmt.Println(`{
			"langs" : ["go"],
			"pass_by" : "stdin"
		}`)
		return
	}
	files := ast2go.File{}
	err := json.NewDecoder(os.Stdin).Decode(&files)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
		return
	}
}
