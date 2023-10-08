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
	file := ast2go.AstFile{}
	err := json.NewDecoder(os.Stdin).Decode(&file)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
		return
	}
	g := ast2go.NewGenerator(os.Stdout)

	err = g.Generate(&file)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
		return
	}

}
