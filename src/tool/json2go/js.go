//go:build js && wasm

package main

import (
	"encoding/json"
	"flag"
	"syscall/js"

	ast2go "github.com/on-keyday/brgen/ast2go/ast"
)

func setFunc() {
	jsGo := js.Global().Get("Go")
	if jsGo.IsUndefined() {
		panic("Go is undefined")
	}
	f := js.FuncOf(func(this js.Value, args []js.Value) any {
		result := map[string]any{
			"stderr": "",
			"stdout": "",
			"code":   0,
		}
		if len(args) < 2 {
			result["stderr"] = "args length is less than 2"
			result["code"] = 1
			return result
		}
		if args[0].Type() != js.TypeString {
			result["stderr"] = "args[0] is not string"
			result["code"] = 1
			return result
		}
		jsArray := js.Global().Get("Array")
		if !args[1].InstanceOf(jsArray) {
			result["stderr"] = "args[1] is not array"
			result["code"] = 1
			return result
		}
		cmdargs := []string{}
		for i := 0; i < args[1].Length(); i++ {
			if args[1].Index(i).Type() != js.TypeString {
				result["stderr"] = "args[1] is not string array"
				result["code"] = 1
				return result
			}
			cmdargs = append(cmdargs, args[1].Index(i).String())
		}
		sourceCode := args[0].String()
		if sourceCode == "" {
			result["stderr"] = "args[0] is empty"
			result["code"] = 1
			return result
		}
		resetFlag()
		err := flag.CommandLine.Parse(cmdargs[1:])
		if err != nil {
			result["stderr"] = err.Error()
			result["code"] = 1
			return result
		}
		file := ast2go.AstFile{}
		err = json.Unmarshal([]byte(sourceCode), &file)
		if err != nil {
			result["stderr"] = err.Error()
			result["code"] = 1
			return result
		}
		g := NewGenerator()
		data, err := g.GenerateAndFormat(&file)
		if err != nil {
			result["stderr"] = err.Error()
			result["code"] = 1
			return result
		}
		result["stdout"] = string(data)
		return result
	})
	proto := jsGo.Get("prototype")
	proto.Set("json2goGenerator", f)
}

func main() {
	c := make(chan struct{})
	setFunc()
	<-c
}
