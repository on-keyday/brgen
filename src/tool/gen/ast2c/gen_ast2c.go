package main

import (
	"io"
	"log"
	"os"
	"path/filepath"
	"strings"

	"github.com/iancoleman/strcase"
	"github.com/on-keyday/brgen/src/tool/gen"
)

func generateHeader(rw io.Writer, defs *gen.Defs, single bool) {
	w := gen.NewWriter(rw)
	w.Printf("// Code generated by gen_ast2c.go. DO NOT EDIT.\n\n")
	if !single {
		w.Printf("#pragma once\n#ifndef __AST_H__\n#define __AST_H__\n\n")
	} else {
		w.Printf("#include<string.h>\n")

	}
	w.Printf("#include <stdint.h>\n")
	/*
		this include imports definition below
		// sync with json_stub.h

		typedef struct json_handlers json_handlers;
		struct json_handlers {
		    void* ctx;
		    void* (*object_get)(json_handlers*, void*, const char* name);
		    void* (*array_get)(json_handlers*, void*, size_t i);
		    size_t (*array_size)(json_handlers*, void*);
		    char* (*string_get_alloc)(json_handlers*, void*);
		    // returns non-zero for success, 0 for error
		    int (*number_get)(json_handlers*, void*, uint64_t* n);
		    // returns 0 or 1. -1 for error
		    int (*boolean_get)(json_handlers*, void*);

		    void* (*alloc)(json_handlers*, size_t size, size_t align);
		};

	*/
	w.Printf("#include %q\n\n", "json_stub.h")

	w.Printf("#ifdef __cplusplus\n")

	w.Printf("extern \"C\" {\n")
	if single {
		w.Printf("#else\n")
		w.Printf("#include<stdalign.h>\n")
	}
	w.Printf("#endif\n\n")
	// define node types
	w.Printf("typedef enum NodeType {\n")
	for _, typ := range defs.NodeTypes {
		w.Printf("\t%s,\n", strings.ToUpper(typ))
	}
	w.Printf("} NodeType;\n\n")
	w.Printf("int NodeType_from_string(const char*, NodeType*);\n")
	w.Printf("const char* NodeType_to_string(NodeType);\n\n")

	// declare all struct types
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
			if len(d.Implements) == 0 && d.Name != "Scope" {
				w.Printf("struct %s {\n", d.Name)
				for _, field := range d.Fields {
					w.Printf("\t%s %s;\n", field.Type.CString(), field.Name)
					if field.Type.IsArray {
						w.Printf("\tsize_t %s_size;\n", field.Name)
					}
				}
				w.Printf("};\n\n")
			}
		case *gen.Enum:
			w.Printf("enum %s {\n", d.Name)
			for _, field := range d.Values {
				w.Printf("\t%s,\n", strings.ToUpper(field.Name))
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
			w.Printf("\tconst NodeType node_type;\n")
			for _, field := range d.Fields {
				w.Printf("\t%s %s;\n", field.Type.CString(), field.Name)
			}
			w.Printf("};\n\n")
		case *gen.Struct:
			if len(d.Implements) == 0 && d.Name != "Scope" {
				continue
			}
			w.Printf("struct %s {\n", d.Name)
			w.Printf("\tconst NodeType node_type;\n")
			for _, field := range d.Fields {
				w.Printf("\t%s %s;\n", field.Type.CString(), field.Name)
				if field.Type.IsArray {
					w.Printf("\tsize_t %s_size;\n", field.Name)
				}
			}
			w.Printf("};\n\n")
		}
	}

	w.Printf("typedef struct RawNode {\n")
	w.Printf("\tconst NodeType node_type;\n")
	w.Printf("\tLoc loc;\n")
	w.Printf("\tvoid* body;\n")
	w.Printf("} RawNode;\n\n")

	w.Printf("typedef struct RawScope {\n")
	w.Printf("\tuint64_t prev;\n")
	w.Printf("\tuint64_t next;\n")
	w.Printf("\tuint64_t branch;\n")
	w.Printf("\tuint64_t* ident;\n")
	w.Printf("\tsize_t ident_size;\n")
	w.Printf("\tint is_global;\n")
	w.Printf("} RawScope;\n\n")

	w.Printf("typedef struct Ast {\n")
	w.Printf("\tRawNode* node;\n")
	w.Printf("\tsize_t node_size;\n")
	w.Printf("\tRawScope* scope;\n")
	w.Printf("\tsize_t scope_size;\n")
	w.Printf("} Ast;\n\n")

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
	w.Printf("int NodeType_from_string(const char* str, NodeType* out) {\n")
	for _, def := range defs.NodeTypes {
		w.Printf("\tif (strcmp(%q, str) == 0) {\n", strings.ToLower(def))
		w.Printf("\t\t*out = %s;\n", strings.ToUpper(def))
		w.Printf("\t\treturn 1;\n")
		w.Printf("\t}\n")
	}
	w.Printf("\treturn 0;\n")
	w.Printf("}\n\n")
	w.Printf("const char* NodeType_to_string(NodeType typ) {\n")
	for _, def := range defs.NodeTypes {
		w.Printf("\tif (typ == %s) {\n", strings.ToUpper(def))
		w.Printf("\t\treturn %q;\n", strings.ToLower(def))
		w.Printf("\t}\n")
	}
	w.Printf("\treturn NULL;\n")
	w.Printf("}\n\n")

	w.Printf("Ast* parse_json_to_ast(json_handlers* h,void* root_obj) {\n")
	w.Printf("\tAst* ast = (Ast*)h->alloc(h, sizeof(Ast), alignof(Ast));\n")
	w.Printf("\tif (!ast) {\n")
	w.Printf("\t\treturn NULL;\n")
	w.Printf("\t}\n")
	w.Printf("\tast->node = NULL;\n")
	w.Printf("\tast->node_size = 0;\n")
	w.Printf("\tast->scope = NULL;\n")
	w.Printf("\tast->scope_size = 0;\n")
	w.Printf("\tvoid* node = h->object_get(h, root_obj, \"node\");\n")
	w.Printf("\tif (!node) {\n")
	w.Printf("\t\tgoto error;\n")
	w.Printf("\t}\n")
	w.Printf("\tif(!h->is_array(h,node)) {\n")
	w.Printf("\t\tgoto error;\n")
	w.Printf("\t}\n")
	w.Printf("\tsize_t node_size = h->array_size(h, node);\n")
	w.Printf("\tast->node = (RawNode*)h->alloc(h, sizeof(RawNode) * node_size, alignof(RawNode));\n")
	w.Printf("\tif (!ast->node) {\n")
	w.Printf("\t\tgoto error;\n")
	w.Printf("\t}\n")
	w.Printf("\tast->node_size = node_size;\n")
	w.Printf("\tsize_t i = 0;\n")
	w.Printf("\tfor (i=0; i < node_size; i++) {\n")
	w.Printf("\t\tvoid* n = h->array_get(h, node, i);\n")
	w.Printf("\t\tif (!n) {\n")
	w.Printf("\t\t\tgoto error;\n")
	w.Printf("\t\t}\n")
	w.Printf("\t\tvoid* loc = h->object_get(h, n, \"loc\");\n")
	w.Printf("\t\tif (!loc) {\n")
	w.Printf("\t\t\tgoto error;\n")
	w.Printf("\t\t}\n")
	w.Printf("\t\tvoid* loc_line = h->object_get(h, loc, \"line\");\n")
	w.Printf("\t\tif (!loc_line) {\n")
	w.Printf("\t\t\tgoto error;\n")
	w.Printf("\t\t}\n")
	w.Printf("\t\tvoid* loc_col = h->object_get(h, loc, \"col\");\n")
	w.Printf("\t\tif (!loc_col) {\n")
	w.Printf("\t\t\tgoto error;\n")
	w.Printf("\t\t}\n")
	w.Printf("\t\tvoid* loc_file = h->object_get(h, loc, \"file\");\n")
	w.Printf("\t\tif (!loc_file) {\n")
	w.Printf("\t\t\tgoto error;\n")
	w.Printf("\t\t}\n")
	w.Printf("\t\tvoid* loc_pos = h->object_get(h, loc, \"pos\");\n")
	w.Printf("\t\tif (!loc_pos) {\n")
	w.Printf("\t\t\tgoto error;\n")
	w.Printf("\t\t}\n")
	w.Printf("\t\tvoid* loc_pos_begin = h->object_get(h, loc_pos, \"begin\");\n")
	w.Printf("\t\tif (!loc_pos_begin) {\n")
	w.Printf("\t\t\tgoto error;\n")
	w.Printf("\t\t}\n")
	w.Printf("\t\tvoid* loc_pos_end = h->object_get(h, loc_pos, \"end\");\n")
	w.Printf("\t\tif (!loc_pos_end) {\n")
	w.Printf("\t\t\tgoto error;\n")
	w.Printf("\t\t}\n")
	w.Printf("\t\tvoid* body = h->object_get(h, n, \"body\");\n")
	w.Printf("\t\tif (!body) {\n")
	w.Printf("\t\t\tgoto error;\n")
	w.Printf("\t\t}\n")
	w.Printf("\t\tif(!h->is_object(h,body)) {\n")
	w.Printf("\t\t\tgoto error;\n")
	w.Printf("\t\t}\n")
	w.Printf("\t\tvoid* node_type = h->object_get(h, n, \"node_type\");\n")
	w.Printf("\t\tif (!node_type) {\n")
	w.Printf("\t\t\tgoto error;\n")
	w.Printf("\t\t}\n")
	w.Printf("\t\tconst char* node_type_str = h->string_get(h, node_type);\n")
	w.Printf("\t\tif (!node_type_str) {\n")
	w.Printf("\t\t\tgoto error;\n")
	w.Printf("\t\t}\n")
	w.Printf("\t\tNodeType typ;\n")
	w.Printf("\t\tif (!NodeType_from_string(node_type_str, &typ)) {\n")
	w.Printf("\t\t\tgoto error;\n")
	w.Printf("\t\t}\n")
	w.Printf("\t\t*(NodeType*)&ast->node[i].node_type = typ;\n")
	w.Printf("\t\tif (!h->number_get(h, loc_line, &ast->node[i].loc.line)) {\n")
	w.Printf("\t\t\tgoto error;\n")
	w.Printf("\t\t}\n")
	w.Printf("\t\tif (!h->number_get(h, loc_col, &ast->node[i].loc.col)) {\n")
	w.Printf("\t\t\tgoto error;\n")
	w.Printf("\t\t}\n")
	w.Printf("\t\tif(!h->number_get(h,loc_file,&ast->node[i].loc.file)) {\n")
	w.Printf("\t\t\tgoto error;\n")
	w.Printf("\t\t}\n")
	w.Printf("\t\tif (!h->number_get(h, loc_pos_begin, &ast->node[i].loc.pos.begin)) {\n")
	w.Printf("\t\t\tgoto error;\n")
	w.Printf("\t\t}\n")
	w.Printf("\t\tif (!h->number_get(h, loc_pos_end, &ast->node[i].loc.pos.end)) {\n")
	w.Printf("\t\t\tgoto error;\n")
	w.Printf("\t\t}\n")
	w.Printf("\t\tast->node[i].body = body;\n")
	w.Printf("\t}\n")
	w.Printf("\tvoid* scope = h->object_get(h, root_obj, \"scope\");\n")
	w.Printf("\tif (!scope) {\n")
	w.Printf("\t\tgoto error;\n")
	w.Printf("\t}\n")
	w.Printf("\tif(!h->is_array(h,scope)) {\n")
	w.Printf("\t\tgoto error;\n")
	w.Printf("\t}\n")
	w.Printf("\tsize_t scope_size = h->array_size(h, scope);\n")
	w.Printf("\tast->scope = (RawScope*)h->alloc(h, sizeof(RawScope) * scope_size, alignof(RawScope));\n")
	w.Printf("\tif (!ast->scope) {\n")
	w.Printf("\t\tgoto error;\n")
	w.Printf("\t}\n")
	w.Printf("\tast->scope_size = scope_size;\n")
	w.Printf("\tsize_t scope_ident_step = 0;\n")
	w.Printf("\tfor (i=0; i < scope_size; i++) {\n")
	w.Printf("\t\tvoid* s = h->array_get(h, scope, i);\n")
	w.Printf("\t\tif (!s) {\n")
	w.Printf("\t\t\tgoto error;\n")
	w.Printf("\t\t}\n")
	w.Printf("\t\tvoid* prev = h->object_get(h, s, \"prev\");\n")
	w.Printf("\t\tif (!prev) {\n")
	w.Printf("\t\t\tgoto error;\n")
	w.Printf("\t\t}\n")
	w.Printf("\t\tvoid* next = h->object_get(h, s, \"next\");\n")
	w.Printf("\t\tif (!next) {\n")
	w.Printf("\t\t\tgoto error;\n")
	w.Printf("\t\t}\n")
	w.Printf("\t\tvoid* branch = h->object_get(h, s, \"branch\");\n")
	w.Printf("\t\tif (!branch) {\n")
	w.Printf("\t\t\tgoto error;\n")
	w.Printf("\t\t}\n")
	w.Printf("\t\tvoid* is_global = h->object_get(h, s, \"is_global\");\n")
	w.Printf("\t\tif (!is_global) {\n")
	w.Printf("\t\t\tgoto error;\n")
	w.Printf("\t\t}\n")
	w.Printf("\t\tvoid* ident = h->object_get(h, s, \"ident\");\n")
	w.Printf("\t\tif (!ident) {\n")
	w.Printf("\t\t\tgoto error;\n")
	w.Printf("\t\t}\n")
	w.Printf("\t\tif(!h->is_array(h,ident)) {\n")
	w.Printf("\t\t\tgoto error;\n")
	w.Printf("\t\t}\n")
	w.Printf("\t\tsize_t ident_size = h->array_size(h, ident);\n")
	w.Printf("\t\tast->scope[i].ident = (uint64_t*)h->alloc(h, sizeof(uint64_t) * ident_size, alignof(uint64_t));\n")
	w.Printf("\t\tif (!ast->scope[i].ident) {\n")
	w.Printf("\t\t\tgoto error;\n")
	w.Printf("\t\t}\n")
	w.Printf("\t\tscope_ident_step++;\n")
	w.Printf("\t\tast->scope[i].ident_size = ident_size;\n")
	w.Printf("\t\tif (!h->number_get(h, prev, &ast->scope[i].prev)) {\n")
	w.Printf("\t\t\tgoto error;\n")
	w.Printf("\t\t}\n")
	w.Printf("\t\tif (!h->number_get(h, next, &ast->scope[i].next)) {\n")
	w.Printf("\t\t\tgoto error;\n")
	w.Printf("\t\t}\n")
	w.Printf("\t\tif (!h->number_get(h, branch, &ast->scope[i].branch)) {\n")
	w.Printf("\t\t\tgoto error;\n")
	w.Printf("\t\t}\n")
	w.Printf("\t\tif (!h->boolean_get(h, is_global)) {\n")
	w.Printf("\t\t\tgoto error;\n")
	w.Printf("\t\t}\n")
	w.Printf("\t\tsize_t j = 0;\n")
	w.Printf("\t\tfor (j=0; j < ident_size; j++) {\n")
	w.Printf("\t\t\tvoid* id = h->array_get(h, ident, j);\n")
	w.Printf("\t\t\tif (!id) {\n")
	w.Printf("\t\t\t\tgoto error;\n")
	w.Printf("\t\t\t}\n")
	w.Printf("\t\t\tif (!h->number_get(h, id, &ast->scope[i].ident[j])) {\n")
	w.Printf("\t\t\t\tgoto error;\n")
	w.Printf("\t\t\t}\n")
	w.Printf("\t\t}\n")
	w.Printf("\t}\n")

	w.Printf("\treturn ast;\n")
	w.Printf("error:\n")
	w.Printf("\th->free(h, ast->node);\n")
	w.Printf("for (i=0; i < scope_ident_step; i++) {\n")
	w.Printf("\th->free(h, ast->scope[i].ident);\n")
	w.Printf("}\n")
	w.Printf("\th->free(h, ast->scope);\n")
	w.Printf("\treturn NULL;\n")
	w.Printf("}\n\n")

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

	defs, err := gen.CollectDefinition(list, strcase.ToSnake, strcase.ToCamel, map[string]string{
		"uint":    "uint64_t",
		"uintptr": "uint64_t",
		"bool":    "int",
		"string":  "char*",
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
