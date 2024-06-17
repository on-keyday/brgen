package main

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	gpath "path"
	"path/filepath"
	"runtime"
	"strings"
	"sync"

	"github.com/on-keyday/brgen/astlib/ast2go/request"
)

type Spec struct {
	Langs     []string `json:"langs"`
	Suffix    []string `json:"suffix"`
	Input     string   `json:"input"`
	Separator string   `json:"separator"`
}

type Result struct {
	Path string
	Data []byte
	Err  error
}

type Generator struct {
	generatorPath string
	outputDir     string
	args          []string
	spec          Spec
	result        chan *Result
	request       chan *Result
	ctx           context.Context
	stderr        io.Writer
	//outputCount       *atomic.Int64
	works             *sync.WaitGroup
	dirBaseSuffixChan chan *DirBaseSuffix
	stdinStream       *request.ProcessClient
}

type DirBaseSuffix struct {
	Dir    string `json:"dir"`
	Base   string `json:"base"`
	Suffix string `json:"suffix"`
}

func printf(stderr io.Writer, format string, args ...interface{}) {
	fmt.Fprintf(stderr, "brgen: %s", fmt.Sprintf(format, args...))
}

func NewGenerator(ctx context.Context, work *sync.WaitGroup, stderr io.Writer, res chan *Result /*outputCount *atomic.Int64,*/, dirBaseSuffixChan chan *DirBaseSuffix) *Generator {
	return &Generator{
		works:             work,
		ctx:               ctx,
		stderr:            stderr,
		request:           make(chan *Result),
		result:            res,
		dirBaseSuffixChan: dirBaseSuffixChan,
	}
}

func (g *Generator) Printf(format string, args ...interface{}) {
	printf(g.stderr, format, args...)
}

func (g *Generator) execGenerator(cmd *exec.Cmd, targetFile string) ([]byte, error) {
	errBuf := bytes.NewBuffer(nil)
	cmd.Stderr = errBuf
	buf := bytes.NewBuffer(nil)
	cmd.Stdout = buf
	g.Printf("execGenerator: starting process: %s\n", targetFile)
	err := cmd.Run()
	g.Printf("execGenerator: done process: %s\n", targetFile)
	if err != nil {
		if errBuf.Len() > 0 {
			return nil, fmt.Errorf("%w: %s", err, errBuf.String())
		}
		return nil, err
	}
	return buf.Bytes(), nil
}

func makeTmpFile(basePath string, data []byte) (path string, err error) {
	base := filepath.Base(basePath)
	fp, err := os.CreateTemp(os.TempDir(), fmt.Sprintf("brgen*_%s.json", base))
	if err != nil {
		return
	}
	defer fp.Close()
	_, err = fp.Write(data)
	if err != nil {
		return
	}
	return fp.Name(), nil
}

func (g *Generator) passAst(filePath string, buffer []byte) ([]byte, error) {
	if g.spec.Input == "stdin" {
		cmd := exec.CommandContext(g.ctx, g.generatorPath, g.args...)
		cmd.Stdin = bytes.NewReader(buffer)
		return g.execGenerator(cmd, filePath)
	}
	path, err := makeTmpFile(filePath, buffer)
	if err != nil {
		return nil, err
	}
	defer os.Remove(path)
	cmd := exec.CommandContext(g.ctx, g.generatorPath, g.args...)
	cmd.Args = append(cmd.Args, path)
	return g.execGenerator(cmd, filePath)
}

var ErrIgnoreMissing = errors.New("ignore missing")

func (g *Generator) sendResult(req *Result) {
	g.result <- req
}

func (g *Generator) sendGenerated(fileBase string, data []byte) {
	suffix := filepath.Ext(fileBase)
	var path string
	if strings.HasPrefix("/dev/stdout", g.outputDir) {
		baseName := filepath.Base(fileBase)
		path = "/dev/stdout/" + baseName
	} else {
		path = filepath.Join(g.outputDir, fileBase)
	}
	g.works.Add(1)
	go func(path string, data []byte, suffix string) {
		defer g.works.Done()
		if runtime.GOOS == "windows" {
			path = strings.ReplaceAll(path, "\\", "/")
		}
		g.dirBaseSuffixChan <- &DirBaseSuffix{
			Dir:    gpath.Dir(path),
			Base:   strings.TrimSuffix(gpath.Base(path), suffix),
			Suffix: suffix,
		}
		g.sendResult(&Result{
			Path: path,
			Data: data,
		})
	}(path, data, suffix)
}

func (g *Generator) handleRequest(req *Result) {
	defer g.works.Done() // this Done is for the generator handlers Add
	if g.stdinStream != nil {
		g.handleStdinStreamRequest(req)
		return
	}
	data, err := g.passAst(req.Path, req.Data)
	if err != nil {
		req.Err = fmt.Errorf("passAst: %s: %w", g.generatorPath, err)
		g.sendResult(req)
		return
	}
	ext := filepath.Ext(req.Path)
	basePath := strings.TrimSuffix(filepath.Base(req.Path), ext)
	var split_data [][]byte
	if len(g.spec.Suffix) == 1 {
		split_data = [][]byte{data}
	} else {
		split_data = bytes.Split(data, []byte(g.spec.Separator))
	}
	if len(split_data) > len(g.spec.Suffix) {
		req.Err = fmt.Errorf("too many output. expect equal to or less than %d, got %d: %s", len(g.spec.Suffix), len(split_data), req.Path)
		g.sendResult(req)
		return
	}
	for i, suffix := range g.spec.Suffix {
		if i >= len(split_data) {
			break
		}
		g.sendGenerated(basePath+suffix, split_data[i])
	}
}

func (g *Generator) run() {
	for {
		select {
		case <-g.ctx.Done():
			return
		case req := <-g.request:
			go g.handleRequest(req)
		}
	}
}

func (g *Generator) StartGenerator(out *Output) error {
	g.generatorPath = out.Generator
	if runtime.GOOS == "windows" {
		if !strings.HasSuffix(g.generatorPath, ".exe") {
			g.generatorPath += ".exe"
		}
	}
	g.outputDir = out.OutputDir
	g.args = out.Args
	err := g.askSpec()
	if err != nil {
		if out.IgnoreMissing {
			return ErrIgnoreMissing
		}
		return fmt.Errorf("askSpec: %s: %w", g.generatorPath, err)
	}
	go g.run()
	return nil
}

func (g *Generator) Request(req *Result) {
	g.request <- req
}

func (g *Generator) askSpec() error {
	cmd := exec.CommandContext(g.ctx, g.generatorPath, "-s")
	cmd.Args = append(cmd.Args, g.args...)
	cmd.Stderr = g.stderr
	buf := bytes.NewBuffer(nil)
	cmd.Stdout = buf
	err := cmd.Run()
	if err != nil {
		return err
	}
	err = json.NewDecoder(buf).Decode(&g.spec)
	if err != nil {
		return err
	}
	if len(g.spec.Langs) == 0 {
		return errors.New("langs is empty")
	}
	if g.spec.Input == "" {
		return errors.New("input is empty")
	}
	if g.spec.Input != "stdin" && g.spec.Input != "file" && g.spec.Input != "stdin_stream" {
		return errors.New("input must be stdin or file or stdin_stream")
	}
	if g.spec.Input == "stdin_stream" {
		return g.initStdinStream()
	} else {
		if len(g.spec.Suffix) == 0 {
			return errors.New("suffix is empty")
		}
		if len(g.spec.Suffix) > 1 && len(g.spec.Separator) == 0 {
			return errors.New("separator is empty when suffix is more than 1")
		}
	}
	return nil
}
