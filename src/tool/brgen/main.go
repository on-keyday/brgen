package main

import (
	"bytes"
	"context"
	"encoding/json"
	"flag"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"sync"

	"github.com/on-keyday/brgen/src/tool/ast2go"
)

type Generator struct {
	w        sync.WaitGroup
	src2json string
	ctx      context.Context
}

func (g *Generator) Init(src2json string) error {
	if src2json != "" {
		g.src2json = src2json
	} else {
		res, err := os.Executable()
		if err != nil {
			return err
		}
		g.src2json = filepath.Join(filepath.Dir(res), "src2json")
		if runtime.GOOS == "windows" {
			g.src2json += ".exe"
		}
	}
	g.ctx = context.TODO()
	return nil
}

func (g *Generator) loadAst(path string) ([]byte, error) {
	cmd := exec.CommandContext(g.ctx, g.src2json, "-s", path)
	cmd.Stderr = os.Stderr
	buf := bytes.NewBuffer(nil)
	cmd.Stdout = buf
	err := cmd.Run()
	if err != nil {
		return nil, err
	}
	return buf.Bytes(), nil
}

func (g *Generator) generate(path string) {
	defer log.Printf("done: %s\n", path)
	buf, err := g.loadAst(path)
	if err != nil {
		log.Printf("loadAst: %s: %s\n", path, err)
		return
	}
	var f []*ast2go.File
	if err := json.Unmarshal(buf, &f); err != nil {
		log.Printf("unmarshal: %s: %s\n", path, err)
		return
	}
}

func (g *Generator) Generate(path string) {
	g.w.Add(1)
	go func() {
		defer g.w.Done()
		g.generate(path)
	}()
}

func (g *Generator) Wait() {
	g.w.Wait()
}

var src2json = flag.String("src2json", "", "path to src2json")

func main() {
	flag.Parse()
	args := flag.Args()
	if len(args) == 0 {
		flag.Usage()
		return
	}
	g := &Generator{}
	if err := g.Init(*src2json); err != nil {
		log.Fatal(err)
	}
	for _, arg := range args {
		g.Generate(arg)
	}
	g.Wait()
}
