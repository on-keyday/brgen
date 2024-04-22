package main

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"io/fs"
	"net/http"
	"os"
	"os/exec"
	"os/signal"
	"path/filepath"
	"runtime"
	"strings"
	"sync"
	"syscall"

	"github.com/on-keyday/brgen/src/tool/s2jgo"
)

type GeneratorHandler struct {
	src2json string
	viaHTTP  *exec.Cmd
	libs2j   *s2jgo.Src2JSON
	ctx      context.Context
	cancel   context.CancelFunc

	resultQueue chan *Result
	errQueue    chan error
	queue       chan *Result

	suffixPattern string
	stderr        io.Writer
	generators    []*Generator
	works         sync.WaitGroup

	dirBaseSuffixChan chan *DirBaseSuffix
	dirBaseSuffix     []*DirBaseSuffix
}

func (g *GeneratorHandler) Printf(format string, args ...interface{}) {
	printf(g.stderr, format, args...)
}

func (g *GeneratorHandler) loadSrc2JSON(src2json, libs2j string) error {
	if libs2j != "" { // try to load libs2j
		if s2jgo.Available() {
			if runtime.GOOS == "windows" {
				if !strings.HasSuffix(libs2j, ".dll") {
					libs2j += ".dll"
				}
			} else {
				if !strings.HasSuffix(libs2j, ".so") {
					libs2j += ".so"
				}
			}
			path, err := filepath.Abs(libs2j)
			if err == nil {
				g.libs2j, err = s2jgo.Load(path)
				if err == nil {
					return nil // success
				}
			}
			g.Printf("libs2j: %s\n", err)
			// skip loading
		} else {
			g.Printf("libs2j: not available\n")
		}
	}
	if src2json != "" {
		g.src2json = src2json
		if runtime.GOOS == "windows" {
			if !strings.HasSuffix(g.src2json, ".exe") {
				g.src2json += ".exe"
			}
		}
	} else {
		res, err := os.Executable()
		if err != nil {
			return fmt.Errorf("os.Executable: %w", err)
		}
		g.src2json = filepath.Join(filepath.Dir(res), "src2json")
		if runtime.GOOS == "windows" {
			g.src2json += ".exe"
		}
	}
	cmd := exec.CommandContext(g.ctx, g.src2json, "--version")
	cmd.Stderr = g.stderr
	cmd.Stdout = g.stderr
	err := cmd.Run()
	if err != nil {
		return fmt.Errorf("src2json: %w", err)
	}
	err = exec.CommandContext(g.ctx, g.src2json, "--check-http").Run()
	if err == nil {
		g.viaHTTP = exec.CommandContext(g.ctx, g.src2json, "--via-http", "--port", "8080")
		g.viaHTTP.Stderr = g.stderr
		g.viaHTTP.Stdout = g.stderr
		err = g.viaHTTP.Start()
		if err != nil {
			return fmt.Errorf("src2json: %w", err)
		}
	}
	return nil
}

func (g *GeneratorHandler) Init(src2json string, libs2j string, output []*Output, suffix string) error {
	if len(output) == 0 {
		return errors.New("output is required")
	}
	g.ctx, g.cancel = signal.NotifyContext(context.Background(), os.Interrupt, os.Kill, syscall.SIGTERM)
	g.suffixPattern = suffix
	g.resultQueue = make(chan *Result, 1)
	g.errQueue = make(chan error, 1)
	g.dirBaseSuffixChan = make(chan *DirBaseSuffix, 1)
	err := g.loadSrc2JSON(src2json, libs2j)
	if err != nil {
		return err
	}
	go func() {
		for {
			select {
			case <-g.ctx.Done():
				return
			case req := <-g.dirBaseSuffixChan:
				g.dirBaseSuffix = append(g.dirBaseSuffix, req)
			}
		}
	}()
	for _, out := range output {
		err := g.dispatchGenerator(out)
		if err != nil {
			g.cancel()
			return err
		}
	}
	return nil
}

func (g *GeneratorHandler) lookupPath(name string) ([]string, error) {
	s, err := os.Stat(name)
	if err != nil {
		return nil, err
	}
	if s.IsDir() {
		if g.suffixPattern == "" {
			return nil, errors.New("suffix is required")
		}
		if strings.Contains(g.suffixPattern, "*") {
			return nil, errors.New("suffix must not contain *")
		}
		files, err := fs.Glob(os.DirFS(name), "./*"+g.suffixPattern)
		if err != nil {
			return nil, err
		}
		if len(files) == 0 {
			return nil, fmt.Errorf("no %s files found", g.suffixPattern)
		}
		for i, file := range files {
			files[i] = filepath.Join(name, file)
		}
		return files, err
	}
	return []string{name}, nil
}

func (g *GeneratorHandler) appendConfig(args []string) []string {
	if config.Warnings.DisableUntypedWarning {
		args = append(args, "--disable-untyped")
	}
	if config.Warnings.DisableUnusedWarning {
		args = append(args, "--disable-unused")
	}
	return args
}

func (g *GeneratorHandler) loadAstHTTP(path string) ([]byte, error) {
	var input struct {
		Args []string `json:"args"`
	}
	input.Args = g.appendConfig([]string{path, "--print-json", "--print-on-error", "--no-color"})
	in, err := json.Marshal(input)
	if err != nil {
		return nil, err
	}
	r, err := http.Post("http://localhost:8080/parse", "application/json", bytes.NewReader(in))
	if err != nil {
		return nil, err
	}
	defer func() {
		io.Copy(io.Discard, r.Body)
		r.Body.Close()
	}()
	data, err := io.ReadAll(r.Body)
	if err != nil {
		return nil, err
	}
	c := r.Header.Get("Content-Type")
	if c != "application/json" {
		return nil, fmt.Errorf("error: %d %s", r.StatusCode, string(data))
	}
	var res struct {
		Stdout   string `json:"stdout"`
		Stderr   string `json:"stderr"`
		ExitCode int    `json:"exit_code"`
	}
	err = json.Unmarshal(data, &res)
	if err != nil {
		return nil, err
	}
	if res.ExitCode != 0 {
		return nil, fmt.Errorf("error: %d %s", res.ExitCode, res.Stderr)
	}
	return []byte(res.Stdout), nil
}

func (g *GeneratorHandler) loadAstLibS2J(path string) ([]byte, error) {
	args := g.appendConfig([]string{path, "--print-json", "--print-on-error", "--no-color"})
	res, err := g.libs2j.Call(args, s2jgo.CAPABILITY_ALL&^s2jgo.CAPABILITY_STDIN&^s2jgo.CAPABILITY_ARGV&^s2jgo.CAPABILITY_NETWORK)
	if err != nil {
		return nil, err
	}
	return res.Stdout, nil
}

func (g *GeneratorHandler) loadAst(path string) ([]byte, error) {
	if g.libs2j != nil {
		return g.loadAstLibS2J(path)
	}
	if g.viaHTTP != nil {
		return g.loadAstHTTP(path)
	}
	return g.loadAstCommand(path)
}

func (g *GeneratorHandler) loadAstCommand(path string) ([]byte, error) {
	cmd := exec.CommandContext(g.ctx, g.src2json, path)
	cmd.Args = g.appendConfig(cmd.Args)
	errBuf := bytes.NewBuffer(nil)
	cmd.Stderr = errBuf
	buf := bytes.NewBuffer(nil)
	cmd.Stdout = buf
	g.Printf("loadAst: starting process: %s\n", path)
	err := cmd.Run()
	g.Printf("loadAst: done process: %s\n", path)
	if err != nil {
		if errBuf.Len() > 0 {
			return nil, fmt.Errorf("%w: %s", err, errBuf.String())
		}
		return nil, err
	}
	return buf.Bytes(), nil
}

func (g *GeneratorHandler) generateAST(path string, reqwg *sync.WaitGroup) {
	files, err := g.lookupPath(path)
	if err != nil {
		g.Printf("lookupPath: %s: %s\n", path, err)
		return
	}
	for _, file := range files {
		g.works.Add(1)
		reqwg.Add(1)
		go func(file string) {
			defer g.works.Done()
			defer reqwg.Done()
			buf, err := g.loadAst(file)
			if err != nil {
				go func() {
					g.errQueue <- fmt.Errorf("loadAst: %s: %w", file, err)
				}()
				return
			}
			for _, gen := range g.generators {
				// this work is Done by the generator
				g.works.Add(1)
				gen.Request(&Result{
					Path: file,
					Data: buf,
				})
			}
		}(file)
	}
}

func (g *GeneratorHandler) dispatchGenerator(out *Output) error {
	gen := NewGenerator(g.ctx, &g.works, g.stderr, g.resultQueue /*&g.outputCount,*/, g.dirBaseSuffixChan)
	err := gen.StartGenerator(out)
	if err != nil {
		if err == ErrIgnoreMissing {
			return nil
		}
		return err
	}
	g.generators = append(g.generators, gen)
	return nil
}

func (g *GeneratorHandler) requestStop() {
	if g.viaHTTP != nil {
		r, err := http.Get("http://localhost:8080/stop")
		if err != nil {
			g.Printf("requestStop: %s\n", err)
			return
		}
		defer r.Body.Close()
		io.Copy(io.Discard, r.Body)
		if err = g.viaHTTP.Wait(); err != nil {
			g.Printf("requestStop: %v\n", err)
		}
	}
}

func (g *GeneratorHandler) StartGenerator(path ...string) {
	g.queue = make(chan *Result, 1)
	reqwg := &sync.WaitGroup{}
	for _, p := range path {
		g.works.Add(1)
		go func(p string) {
			defer g.works.Done()
			g.generateAST(p, reqwg)
		}(p)
	}

	go func() {
		g.works.Wait()
		g.cancel()
	}()

	go func() {
		reqwg.Wait()
		g.requestStop()
	}()

	go func() {
		for {
			select {
			case r := <-g.resultQueue:
				g.queue <- r
			case err := <-g.errQueue:
				g.queue <- &Result{Err: err}
			case <-g.ctx.Done():
				close(g.queue)
				return
			}
		}
	}()
}

func (g *GeneratorHandler) Recv() <-chan *Result {
	return g.queue
}
