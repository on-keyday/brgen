package main

import (
	"io"
	"log"
	"os"
	"path/filepath"

	"github.com/iancoleman/strcase"
	"github.com/on-keyday/brgen/src/tool/gen"
)

func generate(rw io.Writer, defs *gen.Defs) {
	w := gen.NewWriter(rw)
	w.Printf("// Code generated by gen_ast2ts; DO NOT EDIT.\n")
	w.Printf("\n")
	w.Printf("export namespace ast2ts {\n")
	w.Printf("\n")
	defer w.Printf("}\n")

	for _, def := range defs.Defs {
		switch d := def.(type) {
		case *gen.Interface:
			w.Printf("export interface %s", d.Name)
			if d.Embed != "" {
				w.Printf(" extends %s", d.Embed)
			}
			w.Printf(" {\n")
			if d.Name == "Node" {
				w.Printf("	readonly node_type: NodeType;\n")
			}
			for _, field := range d.UnCommonFields {
				w.Printf("	%s: %s;\n", field.Name, field.Type.TsString())
			}
			w.Printf("}\n\n")

			w.Printf("export function is%s(obj: any): obj is %s {\n", d.Name, d.Name)
			for _, types := range d.Derived {
				found, ok := defs.Structs[types]
				if !ok {
					continue
				}
				w.Printf("	if (is%s(obj)) return true;\n", found.Name)
			}
			w.Printf("	return false;\n")
			w.Printf("}\n\n")
		case *gen.Struct:
			w.Printf("export interface %s ", d.Name)
			if len(d.Implements) > 0 {
				w.Printf("extends %s ", d.Implements[0])
			}
			w.Printf("{\n")
			for _, field := range d.UnCommonFields {
				w.Printf("	%s: %s;\n", field.Name, field.Type.TsString())
			}
			w.Printf("}\n\n")
			if len(d.Implements) > 0 {
				w.Printf("export function is%s(obj: any): obj is %s {\n", d.Name, d.Name)
				w.Printf("	return obj && typeof obj === 'object' && typeof obj?.node_type === 'string' && obj.node_type === %q\n", d.NodeType)
				w.Printf("}\n\n")
			} else {
				w.Printf("export function is%s(obj: any): obj is %s {\n", d.Name, d.Name)
				w.Printf("	return obj && typeof obj === 'object' && ")
				for i, field := range d.Fields {
					if i != 0 {
						w.Printf(" && ")
					}
					if field.Type.IsArray {
						if field.Type.IsOptional {
							w.Printf("(obj?.%s === null || Array.isArray(obj?.%s))", field.Name, field.Name)
						} else {
							w.Printf("Array.isArray(obj?.%s)", field.Name)
						}
					} else if field.Type.Name == "Scope" {
						// omit depth check
						w.Printf("typeof obj?.%s === 'object'", field.Name)
					} else if field.Type.Name == "any" {
						w.Printf("obj?.%s !== undefined", field.Name)
					} else if field.Type.Name == "number" || field.Type.Name == "string" || field.Type.Name == "boolean" {
						if field.Type.IsOptional {
							w.Printf("(obj?.%s === null || typeof obj?.%s === %q)", field.Name, field.Name, field.Type.Name)
						} else {
							w.Printf("typeof obj?.%s === %q", field.Name, field.Type.Name)
						}
					} else if field.Type.IsPtr || field.Type.IsInterface || field.Type.IsOptional {
						w.Printf("(obj?.%s === null || is%s(obj?.%s))", field.Name, field.Type.Name, field.Name)
					} else {
						w.Printf("is%s(obj?.%s)", field.Type.Name, field.Name)
					}
				}
				w.Printf("\n")
				w.Printf("}\n\n")
			}

		case *gen.Enum:
			if d.Name == "NodeType" {
				w.Printf("export type NodeType = ")
				for i, val := range d.Values {
					if i != 0 {
						w.Printf(" | ")
					}
					w.Printf("%q", val.Value)
				}
				w.Printf(";\n\n")
			} else {
				w.Printf("export enum %s {\n", d.Name)
				for _, val := range d.Values {
					w.Printf("	%s = %q,\n", val.Name, val.Value)
				}
				w.Printf("};\n\n")
			}
			w.Printf("export function is%s(obj: any): obj is %s {\n", d.Name, d.Name)
			w.Printf("	return obj && typeof obj === 'string' && (")
			for i, val := range d.Values {
				if i != 0 {
					w.Printf(" || ")
				}
				w.Printf("obj === %q", val.Value)
			}
			w.Printf(")\n")
			w.Printf("}\n\n")

		}
	}

	w.Printf("interface astConstructor {\n")
	w.Printf("	node : Node[];\n")
	w.Printf("	scope : Scope[];\n")
	w.Printf("}\n\n")

	w.Printf("export function parseAST(obj: any): Program {\n")
	w.Printf("	if (!isJsonAst(obj)) {\n")
	w.Printf("		throw new Error('invalid ast');\n")
	w.Printf("	}\n")
	w.Printf("	const o :JsonAst = {\n")
	w.Printf("		node: obj.node.map((n: any) => {\n")
	w.Printf("			if (!isRawNode(n)) {\n")
	w.Printf("				throw new Error('invalid node');\n")
	w.Printf("			}\n")
	w.Printf("			return n;\n")
	w.Printf("		}),\n")
	w.Printf("		scope: obj.scope.map((s: any) => {\n")
	w.Printf("			if (!isRawScope(s)) {\n")
	w.Printf("				throw new Error('invalid scope');\n")
	w.Printf("			}\n")
	w.Printf("			return s;\n")
	w.Printf("		})\n")
	w.Printf("	}\n")
	w.Printf("	const c :astConstructor = {\n")
	w.Printf("		node: [],\n")
	w.Printf("		scope: []\n")
	w.Printf("	}\n")
	w.Printf("	for (const on of o.node) {\n")
	w.Printf("		switch (on.node_type) {\n")
	for _, def := range defs.Defs {
		switch d := def.(type) {
		case *gen.Struct:
			if len(d.Implements) == 0 {
				continue
			}
			w.Printf("		case %q: {\n", d.NodeType)
			w.Printf("			const n :%s = {\n", d.Name)
			w.Printf("				node_type: %q,\n", d.NodeType)
			for _, field := range d.Fields {
				if field.Type.IsArray {
					w.Printf("				%s: [],\n", field.Name)
				} else if field.Type.IsPtr || field.Type.IsInterface {
					w.Printf("				%s: null,\n", field.Name)
				} else if field.Type.Name == "number" {
					w.Printf("				%s: 0,\n", field.Name)
				} else if field.Type.Name == "string" {
					w.Printf("				%s: '',\n", field.Name)
				} else if field.Type.Name == "boolean" {
					w.Printf("				%s: false,\n", field.Name)
				} else if field.Type.Name == "Loc" {
					w.Printf("				%s: on.loc,\n", field.Name)
				} else {
					found, ok := defs.Enums[field.Type.Name]
					if !ok {
						continue
					}
					w.Printf("				%s: %s.%s,\n", field.Name, found.Name, found.Values[0].Name)
				}
			}
			w.Printf("			}\n")
			w.Printf("			c.node.push(n);\n")
			w.Printf("			break;\n")
			w.Printf("		}\n")
		}
	}
	w.Printf("		default:\n")
	w.Printf("			throw new Error('invalid node type');\n")
	w.Printf("		}\n")
	w.Printf("	}\n")
	w.Printf("	for (const _ of o.scope) {\n")
	w.Printf("		const n :Scope = {\n")
	for _, field := range defs.ScopeDef.Fields {
		if field.Type.IsArray {
			w.Printf("			%s: [],\n", field.Name)
		} else if field.Type.IsPtr || field.Type.IsInterface {
			w.Printf("			%s: null,\n", field.Name)
		} else if field.Type.Name == "number" {
			w.Printf("			%s: 0,\n", field.Name)
		} else if field.Type.Name == "string" {
			w.Printf("			%s: '',\n", field.Name)
		} else if field.Type.Name == "boolean" {
			w.Printf("			%s: false,\n", field.Name)
		}
	}
	w.Printf("		}\n")
	w.Printf("		c.scope.push(n);\n")
	w.Printf("	}\n")
	w.Printf("	for (let i = 0; i < o.node.length; i++) {\n")
	w.Printf("		const on = o.node[i];\n")
	w.Printf("		const cnode = c.node[i];\n")
	w.Printf("		switch (cnode.node_type) {\n")
	for _, def := range defs.Defs {
		switch d := def.(type) {
		case *gen.Struct:
			if len(d.Implements) == 0 {
				continue
			}
			w.Printf("		case %q: {\n", d.NodeType)
			w.Printf("			const n :%s = cnode as %s;\n", d.Name, d.Name)
			for _, field := range d.Fields {
				if field.Name == "loc" {
					continue
				}
				if field.Type.IsArray {
					w.Printf("			for (const o of on.body.%s) {\n", field.Name)
					w.Printf("				if (typeof o !== 'number') {\n")
					w.Printf("					throw new Error('invalid node list at %s::%s');\n", d.Name, field.Name)
					w.Printf("				}\n")
					w.Printf("				const tmp%s = c.node[o];\n", field.Name)
					if field.Type.Name != "Node" {
						w.Printf("				if (!is%s(tmp%s)) {\n", field.Type.Name, field.Name)
						w.Printf("					throw new Error('invalid node list at %s::%s');\n", d.Name, field.Name)
						w.Printf("				}\n")
					}
					w.Printf("				n.%s.push(tmp%s);\n", field.Name, field.Name)
					w.Printf("			}\n")

				} else if field.Type.Name == "Scope" {
					w.Printf("			if (on.body?.%s !== null && typeof on.body?.%s !== 'number') {\n", field.Name, field.Name)
					w.Printf("				throw new Error('invalid node list at %s::%s');\n", d.Name, field.Name)
					w.Printf("			}\n")
					w.Printf("			const tmp%s = on.body.%s === null ? null : c.scope[on.body.%s];\n", field.Name, field.Name, field.Name)
					w.Printf("			if (tmp%s !== null && !isScope(tmp%s)) {\n", field.Name, field.Name)
					w.Printf("				throw new Error('invalid node list at %s::%s');\n", d.Name, field.Name)
					w.Printf("			}\n")
					w.Printf("			n.%s = tmp%s;\n", field.Name, field.Name)
				} else if field.Type.IsPtr || field.Type.IsInterface {
					w.Printf("			if (on.body?.%s !== null && typeof on.body?.%s !== 'number') {\n", field.Name, field.Name)
					w.Printf("				throw new Error('invalid node list at %s::%s');\n", d.Name, field.Name)
					w.Printf("			}\n")
					w.Printf("			const tmp%s = on.body.%s === null ? null : c.node[on.body.%s];\n", field.Name, field.Name, field.Name)
					w.Printf("			if (!(tmp%s === null || is%s(tmp%s))) {\n", field.Name, field.Type.Name, field.Name)
					w.Printf("				throw new Error('invalid node list at %s::%s');\n", d.Name, field.Name)
					w.Printf("			}\n")
					w.Printf("			n.%s = tmp%s;\n", field.Name, field.Name)
				} else if field.Type.Name == "number" || field.Type.Name == "string" || field.Type.Name == "boolean" {
					w.Printf("			const tmp%s = on.body?.%s;\n", field.Name, field.Name)
					w.Printf("			if (typeof on.body?.%s !== %q) {\n", field.Name, field.Type.Name)
					w.Printf("				throw new Error('invalid node list at %s::%s');\n", d.Name, field.Name)
					w.Printf("			}\n")
					w.Printf("			n.%s = on.body.%s;\n", field.Name, field.Name)
				} else {
					w.Printf("			const tmp%s = on.body?.%s;\n", field.Name, field.Name)
					w.Printf("			if (!is%s(tmp%s)) {\n", field.Type.Name, field.Name)
					w.Printf("				throw new Error('invalid node list at %s::%s');\n", d.Name, field.Name)
					w.Printf("			}\n")
					w.Printf("			n.%s = tmp%s;\n", field.Name, field.Name)
				}
			}
			w.Printf("			break;\n")
			w.Printf("		}\n")
		}
	}
	w.Printf("		}\n")
	w.Printf("	}\n")
	w.Printf("	for (let i = 0; i < o.scope.length; i++) {\n")
	w.Printf("		const os = o.scope[i];\n")
	w.Printf("		const cscope = c.scope[i];\n")
	for _, field := range defs.ScopeDef.Fields {
		if field.Type.IsArray {
			w.Printf("		for (const o of os.%s) {\n", field.Name)
			w.Printf("			if (typeof o !== 'number') {\n")
			w.Printf("				throw new Error('invalid node list at Scope::%s');\n", field.Name)
			w.Printf("			}\n")
			w.Printf("			const tmp%s = c.node[o];\n", field.Name)
			if field.Type.Name != "Node" {
				w.Printf("			if (!is%s(tmp%s)) {\n", field.Type.Name, field.Name)
				w.Printf("				throw new Error('invalid node list at Scope::%s');\n", field.Name)
				w.Printf("			}\n")
			}
			w.Printf("			cscope.%s.push(tmp%s);\n", field.Name, field.Name)
			w.Printf("		}\n")

		} else if field.Type.Name == "Scope" {
			w.Printf("		if (os.%s !== null && typeof os.%s !== 'number') {\n", field.Name, field.Name)
			w.Printf("			throw new Error('invalid node list at Scope::%s');\n", field.Name)
			w.Printf("		}\n")
			w.Printf("		const tmp%s = os.%s === null ? null : c.scope[os.%s];\n", field.Name, field.Name, field.Name)
			w.Printf("		if (tmp%s !== null && !isScope(tmp%s)) {\n", field.Name, field.Name)
			w.Printf("			throw new Error('invalid node list at Scope::%s');\n", field.Name)
			w.Printf("		}\n")
			w.Printf("		cscope.%s = tmp%s;\n", field.Name, field.Name)
		} else if field.Type.Name == "number" || field.Type.Name == "string" || field.Type.Name == "boolean" {
			w.Printf("		cscope.%s = os.%s;\n", field.Name, field.Name)
		}
	}
	w.Printf("	}\n")
	w.Printf("	const root = c.node[0];\n")
	w.Printf("	if (!isProgram(root)) {\n")
	w.Printf("		throw new Error('invalid node list at node[0]');\n")
	w.Printf("	}\n")
	w.Printf("	return root;\n")
	w.Printf("}\n\n")

	w.Printf("export type VisitFn<T> = (f: VisitFn<T>, arg: T) => void|undefined|boolean;\n\n")

	w.Printf("export function walk(node: Node, fn: VisitFn<Node>) {\n")
	w.Printf("	switch (node.node_type) {\n")
	for _, def := range defs.Defs {
		switch d := def.(type) {
		case *gen.Struct:
			if len(d.Implements) == 0 {
				continue
			}
			w.Printf("		case %q: {\n", d.NodeType)
			w.Printf("			if (!is%s(node)) {\n", d.Name)
			w.Printf("				break;\n")
			w.Printf("			}\n")
			w.Printf("			const n :%s = node as %s;\n", d.Name, d.Name)
			for _, field := range d.Fields {
				if field.Type.Name == "Scope" {
					continue
				}
				if field.Type.IsWeak {
					continue // avoid infinite loop
				}
				if field.Type.IsArray {
					w.Printf("			for (const e of n.%s) {\n", field.Name)
					w.Printf("				const result = fn(fn,e);\n")
					w.Printf("				if (result === false) {\n")
					w.Printf("					return;\n")
					w.Printf("				}\n")
					w.Printf("			}\n")
				} else if field.Type.IsPtr || field.Type.IsInterface {
					w.Printf("			if (n.%s !== null) {\n", field.Name)
					w.Printf("				const result = fn(fn,n.%s);\n", field.Name)
					w.Printf("				if (result === false) {\n")
					w.Printf("					return;\n")
					w.Printf("				}\n")
					w.Printf("			}\n")
				}
			}
			w.Printf("			break;\n")
			w.Printf("		}\n")
		}
	}
	w.Printf("	}\n")
	w.Printf("}\n\n")
}

func main() {
	if len(os.Args) != 2 {
		log.Println("usage: gen_ast2ts <file>")
		return
	}

	file := os.Args[1]

	log.SetFlags(log.Flags() | log.Lshortfile)

	list, err := gen.LoadFromDefaultSrc2JSON()

	if err != nil {
		log.Println(err)
		return
	}

	defs, err := gen.CollectDefinition(list, strcase.ToSnake, strcase.ToCamel, map[string]string{
		"uint":    "number",
		"uintptr": "number",
		"bool":    "boolean",
	})

	if err != nil {
		log.Println(err)
		return
	}

	if file == "/dev/stdout" {
		generate(os.Stdout, defs)
		return
	}

	file, err = filepath.Abs(file)

	if err != nil {
		log.Println(err)
		return
	}

	f, err := os.Create(file)

	if err != nil {
		log.Println(err)
		return
	}
	defer f.Close()

	generate(f, defs)

}
