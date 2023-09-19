package main

import (
	"bytes"
	"context"
	"errors"
	"flag"
	"io/fs"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strings"
	"sync"
)

type Generator struct {
	w         sync.WaitGroup
	src2json  string
	json2code string
	ctx       context.Context
}

func (g *Generator) Init(src2json string, json2code string) error {
	if json2code == "" {
		return errors.New("json2code is required")
	}
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

func (g *Generator) lookupPath(name string) ([]string, error) {
	s, err := os.Stat(name)
	if err != nil {
		return nil, err
	}
	if s.IsDir() {
		if *suffix == "" {
			return nil, errors.New("suffix is required")
		}
		if strings.Contains(*suffix, "*") {
			return nil, errors.New("suffix must not contain *")
		}
		files, err := fs.Glob(os.DirFS(name), "./*"+*suffix)
		if err != nil {
			return nil, err
		}
		if len(files) == 0 {
			return nil, errors.New("no .bgn files found")
		}
		return files, err
	}
	return []string{name}, nil
}

func (g *Generator) loadAst(path string) ([]byte, error) {
	files, err := g.lookupPath(path)
	if err != nil {
		return nil, err
	}
	files = append(files, "-s")
	cmd := exec.CommandContext(g.ctx, g.src2json, files...)
	cmd.Stderr = os.Stderr
	buf := bytes.NewBuffer(nil)
	cmd.Stdout = buf
	err = cmd.Run()
	if err != nil {
		return nil, err
	}
	return buf.Bytes(), nil
}

func (g *Generator) passAst(buffer []byte) ([]byte, error) {
	cmd := exec.CommandContext(g.ctx, g.json2code)
	cmd.Stderr = os.Stderr
	cmd.Stdin = bytes.NewReader(buffer)
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
	buf, err = g.passAst(buf)
	if err != nil {
		log.Printf("passAst: %s: %s\n", path, err)
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
var json2code = flag.String("G", "", "alias of json2code")
var suffix = flag.String("suffix", ".bgn", "suffix of file to generate")

func init() {
	flag.StringVar(json2code, "json2code", "", "path to json2code")
}

func main() {
	flag.Parse()
	args := flag.Args()
	if len(args) == 0 {
		flag.Usage()
		return
	}
	g := &Generator{}
	if err := g.Init(*src2json, *json2code); err != nil {
		log.Fatal(err)
	}
	for _, arg := range args {
		g.Generate(arg)
	}
	g.Wait()
}
