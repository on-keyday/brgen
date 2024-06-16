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

	ast "github.com/on-keyday/brgen/ast2go/ast"
	"github.com/on-keyday/brgen/ast2go/gen"
	"github.com/on-keyday/brgen/ast2go/request"
)

var spec = flag.Bool("s", false, "spec of this generator")
var fileName = flag.Bool("f", false, "file to generate")
var legacyStdin = flag.Bool("stdin", false, "use stdin")
var asCodeBlock = flag.Bool("as-code-block", false, "output as code block")
var format = flag.Bool("format", false, "output only format and enum")

func resolveTypeName(a reflect.Type) string {
	n := a.Name()
	if n == "" {
		switch a.Kind() {
		case reflect.Ptr:
			return "*" + resolveTypeName(a.Elem())
		case reflect.Slice:
			return "[]" + resolveTypeName(a.Elem())
		case reflect.Map:
			return "map[" + resolveTypeName(a.Key()) + "]" + resolveTypeName(a.Elem())
		case reflect.Interface:
			return "interface{}"
		default:
			return a.Kind().String()
		}
	}
	return n
}

func diagram(out io.Writer, prog *ast.Program) {

	fmt.Fprintf(out, "erDiagram\n")
	var nodeMap = map[ast.Node]struct{}{}
	visit := ast.VisitFn(func(v ast.Visitor, n ast.Node) bool {
		if _, ok := nodeMap[n]; ok {
			return false
		}
		nodeMap[n] = struct{}{}
		node := reflect.ValueOf(n)
		typ := node.Elem().Type()
		nodePtr := node.Pointer()
		nodeRep := fmt.Sprintf("%s_0x%x", typ.Name(), nodePtr)
		w := bytes.NewBuffer(nil)
		fmt.Fprintf(out, "%s {\n", nodeRep)
		node = node.Elem()
		writeSingleField := func(name string, field reflect.Value) {
			// type
			fmt.Fprintf(out, " %s ", resolveTypeName(field.Type()))
			// field name
			fmt.Fprintf(out, "  %s ", name)
			// value
			if field.Kind() == reflect.Interface {
				field = field.Elem()
				if !field.IsValid() {
					fmt.Fprintf(out, "\"nil\"\n")
					return
				}
			}
			fieldInfo := field.Type()
			switch field.Kind() {
			case reflect.Ptr:
				ptr := field.Pointer()
				elem := fieldInfo.Elem()
				if field.Elem().CanUint() {
					fmt.Fprintf(out, "\"%d\"", field.Elem().Uint())
				} else {
					if ptr == 0 {
						fmt.Fprintf(out, "\"nil\"\n")
						return
					}
					ptrRep := fmt.Sprintf("%s_0x%x", elem.Name(), ptr)
					fmt.Fprintf(out, "%q", ptrRep)
					fmt.Fprintf(w, "%s ||--|o %s :has\n", nodeRep, ptrRep)
				}
			case reflect.Bool:
				fmt.Fprintf(out, "\"%t\"", field.Bool())
			default:
				if s, ok := field.Interface().(fmt.Stringer); ok {
					fmt.Fprintf(out, "%q", s.String())
				} else if field.CanUint() {
					fmt.Fprintf(out, "\"%d\"", field.Uint())
				} else if field.Kind() == reflect.String {
					fmt.Fprintf(out, "%q", field.String())
				} else if field.Type().Name() == "Loc" {
					loc := field.Interface().(ast.Loc)
					fmt.Fprintf(out, "\"%d:%d:%d\"", loc.File, loc.Line, loc.Col)
				}
			}
			fmt.Fprintf(out, "\n")
		}
		for i := 0; i < node.NumField(); i++ {
			field := node.Field(i)
			fieldType := typ.Field(i)
			if fieldType.Type.Kind() == reflect.Slice {
				for j := 0; j < field.Len(); j++ {
					writeSingleField(fmt.Sprintf("%s_%d", fieldType.Name, j), field.Index(j))
				}
			} else {
				writeSingleField(fieldType.Name, field)
			}
		}
		fmt.Fprintf(out, "}\n")
		io.Copy(out, w)
		ast.Walk(n, v)
		return true
	})
	visit(visit, prog)
}

func baseTypeLink(out io.Writer, from string, t ast.Type) {
	if a, ok := t.(*ast.ArrayType); ok {
		baseTypeLink(out, from, a.ElementType)
		return
	}

	if i, ok := t.(*ast.IdentType); ok {
		fmt.Fprintf(out, "%s ||--|{ %s :has\n", from, i.Ident.Ident)
	}
}

func structTypeTrace(out io.Writer, name string, traceTarget *ast.StructType) {
	s := gen.NewExprStringer()
	s.TypeProvider = func(t ast.Type) string {
		if arr, ok := t.(*ast.ArrayType); ok {
			return fmt.Sprintf("%s[%s]", s.TypeProvider(arr.ElementType), s.ExprString(arr.Length))
		}
		if _, ok := t.(*ast.StrLiteralType); ok {
			return "fixed_string"
		}
		return s.GetType(t)
	}
	w := bytes.NewBuffer(nil)
	fmt.Fprintf(out, "%s {\n", name)
	for i := range traceTarget.Fields {
		field := traceTarget.Fields[i]
		switch v := field.(type) {
		case *ast.Field:
			typ := s.TypeProvider(v.FieldType)
			fmt.Fprintf(out, "%s ", typ)
			if v.Ident == nil {
				fmt.Fprintf(out, "anonymous_%d ", i)
			} else {
				fmt.Fprintf(out, "%s\n", v.Ident.Ident)
			}
			baseTypeLink(w, name, v.FieldType)
		case *ast.Function:
			fmt.Fprintf(out, "fn %s\n", v.Ident.Ident)
		}
	}
	fmt.Fprintf(out, "}\n")
	io.Copy(out, w)
}

func traceFormat(out io.Writer, prog *ast.Program) {
	fmt.Fprintf(out, "graph Program {\n")
	visited := map[ast.Node]struct{}{}
	ast.Walk(prog, ast.VisitFn(func(v ast.Visitor, n ast.Node) bool {
		if _, ok := visited[n]; ok {
			return false
		}
		visited[n] = struct{}{}
		switch v := n.(type) {
		case *ast.Format:
			structTypeTrace(out, v.Ident.Ident, v.Body.StructType)
		case *ast.Enum:
			fmt.Fprintf(out, "%s {\n", v.Ident.Ident)
			for i := range v.Members {
				fmt.Fprintf(out, "%s %s\n", v.Members[i].Ident.Ident, v.Ident.Ident)
			}
			fmt.Fprintf(out, "}\n")
		}

		return true
	}))
}

func generate(out io.Writer, in io.Reader) error {
	file := &ast.AstFile{}
	err := json.NewDecoder(in).Decode(file)
	if err != nil {
		return err
	}
	prog, err := ast.ParseAST(file.Ast)
	if err != nil {
		return err
	}
	if *asCodeBlock {
		fmt.Fprintf(out, "```mermaid\n")
		defer fmt.Fprintf(out, "```\n")
	}
	if *format {
		traceFormat(out, prog)
		return nil
	}
	diagram(out, prog)
	return nil
}

func streamMode() {
	err := request.Run(os.Stdin, os.Stdout, func(stream *request.IDStream, req *request.GenerateSource) {
		r := bytes.NewReader(req.JsonText)
		w := bytes.NewBuffer(nil)
		err := generate(w, r)
		if err != nil {
			stream.RespondError(err.Error())
		} else {
			stream.RespondSource(string(req.Name)+".dot", w.Bytes(), "")
		}
	})
	if err != nil {
		log.Fatal(err)
	}
}

func main() {
	flag.Parse()
	if *spec {
		if *legacyStdin {
			fmt.Fprintf(os.Stdout, `
			{
				"input": "stdin",
				"langs": ["graphviz"],
				"suffix": [".dot"]
			}
		`)
		} else if *fileName {
			fmt.Fprintf(os.Stdout, `
			{
				"input": "file",
				"langs": ["graphviz"],
				"suffix": [".dot"]
			}
		`)
		} else {
			fmt.Fprintf(os.Stdout, `
			{
				"input": "stdin_stream",
				"langs": ["graphviz"]
			}
		`)
		}
		return
	}
	if !*legacyStdin && !*fileName {
		streamMode()
		return
	}

	var f *os.File
	if !*fileName {
		f = os.Stdin
	} else {
		if flag.NArg() != 1 {
			log.Fatal("no filename")
		}
		f, err := os.Open(flag.Arg(0))
		if err != nil {
			log.Fatal(err)
		}
		defer f.Close()
	}
	generate(os.Stdout, f)
}
