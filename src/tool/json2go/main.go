package main

import (
	"encoding/json"
	"fmt"
	"os"

	"github.com/on-keyday/brgen/ast2go"
)

func main() {
	files := []ast2go.File{}
	err := json.NewDecoder(os.Stdin).Decode(&files)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
		return
	}
	// config.go.package=xxxx in global scope means golang package name
	// path = config.import("path") means import path into path object
}
