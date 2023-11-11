package main

import (
	"fmt"
	"io"
	"log"
	"os"
	"path/filepath"
	"strings"

	"github.com/iancoleman/strcase"
	"github.com/on-keyday/brgen/src/tool/gen"
)

const prefix = "ast2c_"

func Prefixed(name string) string {
	return prefix + name
}

func generateHeader(rw io.Writer, defs *gen.Defs, single bool) {
	w := gen.NewWriter(rw)
	w.Printf("// Code generated by gen_ast2c.go. DO NOT EDIT.\n\n")
	if !single {
		w.Printf("#pragma once\n#ifndef __AST_H__\n#define __AST_H__\n\n")
	} else {
		w.Printf("#include<string.h>\n")

	}
	w.Printf("#include <stdint.h>\n")
	w.Printf("#ifdef __cplusplus\n")

	w.Printf("extern \"C\" {\n")
	if single {
		w.Printf("#else\n")
		w.Printf("#include<stdalign.h>\n")
	}
	w.Printf("#endif\n\n")
	w.Printf(`
typedef struct %[1]s %[1]s;
struct %[1]s {
	void* ctx;
	void* (*object_get)(%[1]s*, void* obj, const char* name);
	void* (*array_get)(%[1]s*, void* obj, size_t i);
	// return non-zero for success, 0 for error
	int (*array_size)(%[1]s*, void* obj, size_t* size);
	int (*is_null)(%[1]s*, void* obj);
	int (*is_array)(%[1]s*, void* obj);
	int (*is_object)(%[1]s*, void* obj);
	char* (*string_get_alloc)(%[1]s*, void* obj);
	const char* (*string_get)(%[1]s*, void* obj);
	// returns non-zero for success, 0 for error
	int (*number_get)(%[1]s*, void* obj, uint64_t* n);
	// returns 0 or 1. -1 for error
	int (*boolean_get)(%[1]s*, void* obj);

	void* (*alloc)(%[1]s*, size_t size, size_t align);
	void (*free)(%[1]s*, void* ptr);

	void (*error)(%[1]s*, void* ptr, const char* msg);
};

`, Prefixed("json_handlers"))
	// declare all struct types
	w.Printf("typedef struct %s %s;\n", Prefixed("Ast"), Prefixed("Ast"))
	for _, def := range defs.Defs {
		switch d := def.(type) {
		case *gen.Interface:
			w.Printf("typedef struct %s %s;\n", d.Name, d.Name)
		case *gen.Struct:
			w.Printf("typedef struct %s %s;\n", d.Name, d.Name)
		case *gen.Enum:
			w.Printf("typedef enum %s %s;\n", d.Name, d.Name)
		}
	}
	// next non pointer or interface types
	for _, def := range defs.Defs {
		switch d := def.(type) {
		case *gen.Interface:
			continue
		case *gen.Struct:
			if len(d.Implements) == 0 && d.Name != Prefixed("Scope") {
				w.Printf("struct %s {\n", d.Name)
				for _, field := range d.Fields {
					w.Printf("\t%s %s;\n", field.Type.CString(), field.Name)
					if field.Type.IsArray {
						w.Printf("\tsize_t %s_size;\n", field.Name)
					}
				}
				w.Printf("};\n\n")
				if d.Name == Prefixed("Loc") || d.Name == Prefixed("Pos") {
					w.Printf("// returns 1 if succeed 0 if failed\n")
					w.Printf("int %s_parse(%s*,%s*,void*);\n\n", d.Name, d.Name, Prefixed("json_handlers"))
				}
			}
		case *gen.Enum:
			w.Printf("enum %s {\n", d.Name)
			for _, field := range d.Values {
				field.Name = strings.ToUpper(d.Name) + "_" + strings.ToUpper(field.Name)
				w.Printf("\t%s,\n", field.Name)
			}
			w.Printf("};\n")
			w.Printf("const char* %s_to_string(%s);\n", d.Name, d.Name)
			w.Printf("int %s_from_string(const char*,%s*);\n\n", d.Name, d.Name)
		}
	}
	// next pointer types
	for _, def := range defs.Defs {
		switch d := def.(type) {
		case *gen.Interface:
			w.Printf("struct %s {\n", d.Name)
			w.Printf("\tconst ast2c_NodeType node_type;\n")
			for _, field := range d.Fields {
				w.Printf("\t%s %s;\n", field.Type.CString(), field.Name)
			}
			w.Printf("};\n\n")
		case *gen.Struct:
			if len(d.Implements) == 0 && d.Name != Prefixed("Scope") {
				continue
			}
			w.Printf("struct %s {\n", d.Name)
			w.Printf("\tconst %s node_type;\n", Prefixed("NodeType"))
			for _, field := range d.Fields {
				w.Printf("\t%s %s;\n", field.Type.CString(), field.Name)
				if field.Type.IsArray {
					w.Printf("\tsize_t %s_size;\n", field.Name)
				}
			}
			w.Printf("};\n\n")
			w.Printf("// returns 1 if succeed 0 if failed\n")
			w.Printf("int %s_parse(%s* ,%s*,%s*,void*);\n\n", d.Name, Prefixed("Ast"), d.Name, Prefixed("json_handlers"))
		}
	}

	w.Printf("struct %s {\n", Prefixed("Ast"))
	w.Printf("\t%s** nodes;\n", Prefixed("Node"))
	w.Printf("\tsize_t nodes_size;\n")
	w.Printf("\t%s** scopes;\n", Prefixed("Scope"))
	w.Printf("\tsize_t scopes_size;\n")
	w.Printf("};\n\n")

	if !single {
		w.Printf("#ifdef __cplusplus\n")
		w.Printf("}\n")
		w.Printf("#endif\n\n")
		w.Printf("#endif\n")
	}
}

func generateSource(rw io.Writer, defs *gen.Defs, single bool) {
	w := gen.NewWriter(rw)
	if !single {
		w.Printf("// Code generated by gen_ast2c.go. DO NOT EDIT.\n\n")
		w.Printf("#include \"ast.h\"\n")
		w.Printf("#include<string.h>\n\n")
		w.Printf("#ifdef __cplusplus\n")
		w.Printf("extern \"C\" {\n")
		w.Printf("#else\n")
		w.Printf("#include<stdalign.h>\n")
		w.Printf("#endif\n\n")

	}

	for _, def := range defs.Defs {
		switch v := def.(type) {
		case *gen.Enum:
			w.Printf("const char* %s_to_string(%s val) {\n", v.Name, v.Name)
			w.Printf("\tswitch(val) {\n")
			for _, field := range v.Values {
				w.Printf("\tcase %s: return %q;\n", field.Name, field.Value)
			}
			w.Printf("\tdefault: return NULL;\n")
			w.Printf("\t}\n")
			w.Printf("}\n\n")
			w.Printf("// returns 1 if succeed 0 if failed\n")
			w.Printf("int %s_from_string(const char* str, %s* out) {\n", v.Name, v.Name)
			w.Printf("\tif (!str||!out) return 0;\n")
			for _, field := range v.Values {
				w.Printf("\tif (strcmp(str, %q) == 0) {\n", field.Value)
				w.Printf("\t\t*out = %s;\n", field.Name)
				w.Printf("\t\treturn 1;\n")
				w.Printf("\t}\n")
			}
			w.Printf("\treturn 0;\n")
			w.Printf("}\n\n")
		}
	}

	getAlloc := func(typ string) string {
		return "(" + typ + "*)h->alloc(h, sizeof(" + typ + "), alignof(" + typ + "))"
	}

	getObj := func(target, name string) string {
		return fmt.Sprintf("h->object_get(h, %s, %q)", target, name)
	}

	checkObj := func(target string) string {
		return fmt.Sprintf("if (!%s) { goto error; }", target)
	}

	reportError := func(target, err string, args ...any) string {
		return fmt.Sprintf("if(h->error) { h->error(h,%s, %q); }", target, fmt.Sprintf(err, args...))
	}

	checkObjNull := func(structName, target string) string {
		return fmt.Sprintf("if (!%s) { %s return 0; }", target, reportError(target, "%s::%s is null", structName, target))
	}

	initObj := func(to, from, name string) string {
		return fmt.Sprintf("void* %s = %s;", to, getObj(from, name))
	}

	_ = checkObj
	_ = initObj
	_ = getAlloc

	writeFieldsChecks := func(structName string, base string, fields []*gen.Field) {
		for _, field := range fields {
			if field.Type.Name == "char*" || field.Type.IsArray || field.Type.IsInterface || field.Type.IsPtr || field.Type.IsOptional {
				w.Printf("\ts->%s = NULL;\n", field.Name)
			}
		}

		for _, field := range fields {
			if field.Name == "loc" {
				continue
			}
			w.Printf("\t%s\n", initObj(field.Name, base, field.Name))
		}

		for _, field := range fields {
			w.Printf("\t%s\n", checkObjNull(structName, field.Name))
			if field.Type.IsArray {
				w.Printf("\tif(!h->array_size(h, %s,&s->%s_size)) {\n", field.Name, field.Name)
				w.Printf("\t\t%s\n", reportError(field.Name, fmt.Sprintf("failed to get array size of %s::%s", structName, field.Name)))
				w.Printf("\t\treturn NULL;\n")
				w.Printf("\t}\n")
			}
		}
	}

	for _, def := range defs.Defs {
		switch v := def.(type) {
		case *gen.Struct:
			if len(v.Implements) == 0 &&
				v.Name != Prefixed("Loc") && v.Name != Prefixed("Pos") {
				continue
			}
			w.Printf("// returns 1 if succeed 0 if failed\n")
			if len(v.Implements) == 0 {
				w.Printf("int %s_parse(%s* s,%s* h, void* obj) {\n", v.Name, v.Name, Prefixed("json_handlers"))
				writeFieldsChecks(v.Name, "obj", v.Fields)
			} else {
				w.Printf("int %s_parse(%s* ast,%s* s,%s* h, void* obj) {\n", v.Name, Prefixed("Ast"), v.Name, Prefixed("json_handlers"))
				w.Printf("\tif (!ast||!s||!h||!obj) {\n")
				w.Printf("\t\t%s\n", reportError("NULL", "invalid argument"))
				w.Printf("\t\treturn 0;\n")
				w.Printf("\t}\n")
				w.Printf("\t%s\n", initObj("loc", "obj", "loc"))
				w.Printf("\t%s\n", initObj("obj_body", "obj", "body"))
				w.Printf("\t%s\n", checkObjNull("RawNode", "obj_body"))
				writeFieldsChecks(v.Name, "obj_body", v.Fields)
			}

			for _, field := range v.Fields {
				if field.Type.Name == Prefixed("Loc") || field.Type.Name == Prefixed("Pos") {
					w.Printf("\tif(!%s_parse(&s->%s,h,%s)) {\n", field.Type.Name, field.Name, field.Name)
					w.Printf("\t\t%s\n", reportError(field.Name, "failed to parse %s::%s", v.Name, field.Name))
					w.Printf("\t\tgoto error;\n")
					w.Printf("\t}\n")
				} else if field.Type.Name == "uint64_t" {
					w.Printf("\tif(!h->number_get(h,%s,&s->%s)) {\n", field.Name, field.Name)
					w.Printf("\t\t%s\n", reportError(field.Name, "failed to parse %s::%s", v.Name, field.Name))
					w.Printf("\t\tgoto error;\n")
					w.Printf("\t}\n")
				} else if field.Type.Name == "char*" {
					w.Printf("\ts->%s = h->string_get_alloc(h,%s);\n", field.Name, field.Name)
					w.Printf("\tif (!s->%s) {\n", field.Name)
					w.Printf("\t\t%s\n", reportError(field.Name, "failed to parse %s::%s", v.Name, field.Name))
					w.Printf("\t\tgoto error;\n")
					w.Printf("\t}\n")
				}
			}
			w.Printf("\treturn 1;\n")
			w.Printf("error:\n")
			w.Printf("\treturn 0;\n")
			w.Printf("}\n\n")
		}
	}

	w.Printf("#ifdef __cplusplus\n")
	w.Printf("}\n")
	w.Printf("#endif\n\n")

}

func main() {
	if len(os.Args) != 2 && len(os.Args) != 3 {
		log.Println("usage: gen_ast2c [<header file> <source file>] | /dev/stdout")
		return
	}

	hdr := os.Args[1]
	var src string
	if len(os.Args) == 3 {
		src = os.Args[2]
	} else {
		src = hdr
	}

	log.SetFlags(log.Flags() | log.Lshortfile)

	list, err := gen.LoadFromDefaultSrc2JSON()

	if err != nil {
		log.Println(err)
		return
	}

	defs, err := gen.CollectDefinition(list, func(s string) string {
		return strcase.ToSnake(s)
	}, func(s string) string {
		return Prefixed(strcase.ToCamel(s))
	}, map[string]string{
		"uint":    "uint64_t",
		"uintptr": "uintptr_t",
		"bool":    "int",
		"string":  "char*",
		"any":     "void*",
	})

	if err != nil {
		log.Println(err)
		return
	}

	if hdr == "/dev/stdout" {
		generateHeader(os.Stdout, defs, false)
		generateSource(os.Stdout, defs, false)
		return
	}

	hdr, err = filepath.Abs(hdr)

	if err != nil {
		log.Println(err)
		return
	}

	src, err = filepath.Abs(src)
	if err != nil {
		log.Println(err)
		return
	}

	f, err := os.Create(hdr)

	if err != nil {
		log.Println(err)
		return
	}
	defer f.Close()

	if hdr == src {
		generateHeader(f, defs, true)
		generateSource(f, defs, true)
		return
	}

	f2, err := os.Create(src)
	if err != nil {
		log.Println(err)
		return
	}
	defer f2.Close()

	generateHeader(f, defs, false)
	generateSource(f2, defs, false)

}
