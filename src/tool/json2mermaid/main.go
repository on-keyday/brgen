package main

import (
	"bytes"
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"log"
	"os"
	"reflect"

	"github.com/iancoleman/strcase"
	ast "github.com/on-keyday/brgen/ast2go/ast"
)

var spec = flag.Bool("s", false, "spec of this generator")
var fileName = flag.String("f", "", "file to generate")

func main() {
	flag.Parse()
	if *spec {
		fmt.Fprintf(os.Stdout, `
			{
				"input": "stdin",
				"langs": ["mermaid"],
				"suffix": [".md"]
			}
		`)
		return
	}
	file := &ast.AstFile{}
	if *fileName == "" {
		err := json.NewDecoder(os.Stdin).Decode(file)
		if err != nil {
			log.Fatal(err)
		}
	} else {
		f, err := os.Open(*fileName)
		if err != nil {
			log.Fatal(err)
		}
		defer f.Close()
		err = json.NewDecoder(f).Decode(file)
		if err != nil {
			log.Fatal(err)
		}
	}
	prog, err := ast.ParseAST(file.Ast)
	if err != nil {
		log.Fatal(err)
	}
	out := os.Stdout
	fmt.Fprintf(out, "erDiagram\n")
	visit := ast.VisitFn(func(v ast.Visitor, n ast.Node) bool {
		node := reflect.ValueOf(n)
		typ := node.Elem().Type()
		nodePtr := node.Pointer()
		nodeRep := fmt.Sprintf("%s（0x%x）", typ.Name(), nodePtr)
		w := bytes.NewBuffer(nil)
		fmt.Fprintf(out, "%s {\n", nodeRep)
		node = node.Elem()
		for i := 0; i < node.NumField(); i++ {
			field := node.Field(i)
			// field name
			fmt.Fprintf(out, "  %s ", strcase.ToCamel(typ.Field(i).Name))
			// type
			fmt.Fprintf(out, " %s ", field.Type().Name())
			// value
			if field.Kind() == reflect.Interface {
				field = field.Elem()
			}
			fieldInfo := field.Type()
			switch field.Kind() {
			case reflect.Ptr:
				ptr := field.Pointer()
				elem := fieldInfo.Elem()
				if field.Elem().CanInt() {
					fmt.Fprintf(out, "%d", field.Elem().Int())
				} else {
					ptrRep := fmt.Sprintf("%s（0x%x）", elem.Name(), ptr)
					fmt.Fprintf(out, "%s", ptrRep)
					fmt.Fprintf(w, "%s -> %s\n", nodeRep, ptrRep)
				}
			case reflect.Bool:
				fmt.Fprintf(out, "%t", field.Bool())
			default:
				if s, ok := field.Interface().(fmt.Stringer); ok {
					fmt.Fprintf(out, "%s", s.String())
				} else if field.CanInt() {
					fmt.Fprintf(out, "%d", field.Int())
				} else {
					fmt.Fprintf(out, "%s", field.String())
				}
			}
			fmt.Fprintf(out, "\n")
		}
		fmt.Fprintf(out, "}\n")
		io.Copy(out, w)
		ast.Walk(n, v)
		return true
	})
	visit(visit, prog)
}
