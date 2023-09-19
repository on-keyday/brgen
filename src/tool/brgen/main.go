package main

import (
	"bytes"
	"context"
	"flag"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"sync"
)

type Generator struct {
	w        sync.WaitGroup
	src2json string
}

func (g *Generator) Init() error {
	res, err := os.Executable()
	if err != nil {
		return err
	}
	g.src2json = filepath.Join(filepath.Dir(res), "src2json")
	if runtime.GOOS == "windows" {
		g.src2json += ".exe"
	}
	return nil
}

func (g *Generator) loadAst(path string) error {
	ctx := context.TODO()
	cmd := exec.CommandContext(ctx, g.src2json, path)
	cmd.Stderr = os.Stderr
	buf := bytes.NewBuffer(nil)
	cmd.Stdout = buf
	err := cmd.Run()
	if err != nil {
		return err
	}
	return nil
}

func (g *Generator) generate(path string) {
	defer log.Printf("done: %s\n", path)
	if err := g.loadAst(path); err != nil {
		log.Printf("loadAst: %s: %s\n", path, err)
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

func main() {
	flag.Parse()
	args := flag.Args()
	if len(args) == 0 {
		flag.Usage()
		return
	}
	g := &Generator{}
	if err := g.Init(); err != nil {
		log.Fatal(err)
	}
	for _, arg := range args {
		g.Generate(arg)
	}
	g.Wait()
}
