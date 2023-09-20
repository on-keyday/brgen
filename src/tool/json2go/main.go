package main

import (
	"encoding/json"
	"fmt"
	"os"

	"github.com/on-keyday/brgen/ast2go"
)

func main() {
	files := ast2go.File{}
	err := json.NewDecoder(os.Stdin).Decode(&files)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
		return
	}
}
