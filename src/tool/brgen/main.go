package main

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io"
	"io/fs"
	"log"
	"os"
	"os/exec"
	"os/signal"
	"path/filepath"
	"runtime"
	"strings"
	"sync"
	"sync/atomic"
	"time"
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
	outputCount   *atomic.Int64
}

type GeneratorHandler struct {
	w         sync.WaitGroup
	src2json  string
	json2code string
	ctx       context.Context
	cancel    context.CancelFunc

	resultQueue chan *Result
	errQueue    chan error
	queue       chan *Result

	suffixPattern string
	stderr        io.Writer
	generators    []*Generator
	outputCount   atomic.Int64
}

func printf(stderr io.Writer, format string, args ...interface{}) {
	fmt.Fprintf(stderr, "brgen: %s", fmt.Sprintf(format, args...))
}

func (g *GeneratorHandler) Printf(format string, args ...interface{}) {
	printf(g.stderr, format, args...)
}

func (g *GeneratorHandler) Init(src2json string, output []*Output, suffix string) error {
	if len(output) == 0 {
		return errors.New("output is required")
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
	g.ctx, g.cancel = signal.NotifyContext(context.Background(), os.Interrupt)
	g.suffixPattern = suffix
	g.resultQueue = make(chan *Result, 1)
	g.errQueue = make(chan error, 1)
	for _, out := range output {
		err := g.dispatchGenerator(out)
		if err != nil {
			g.cancel()
			return err
		}
	}
	return nil
}

func NewGenerator(ctx context.Context, work *sync.WaitGroup, stderr io.Writer, res chan *Result, outputCount *atomic.Int64) *Generator {
	return &Generator{
		ctx:         ctx,
		stderr:      stderr,
		request:     make(chan *Result, 1),
		result:      res,
		outputCount: outputCount,
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

func makeTmpFile(data []byte) (path string, err error) {
	fp, err := os.CreateTemp(os.TempDir(), "brgen*.json")
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
	path, err := makeTmpFile(buffer)
	if err != nil {
		return nil, err
	}
	defer os.Remove(path)
	cmd := exec.CommandContext(g.ctx, g.generatorPath, g.args...)
	cmd.Args = append(cmd.Args, path)
	return g.execGenerator(cmd, filePath)
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
		return fmt.Errorf("askSpec: %s: %w", g.generatorPath, err)
	}
	go func() {
		for {
			select {
			case <-g.ctx.Done():
				return
			case req := <-g.request:
				go func() {
					data, err := g.passAst(req.Path, req.Data)
					if err != nil {
						req.Err = fmt.Errorf("passAst: %s: %w", g.generatorPath, err)
						g.result <- req
						return
					}
					ext := filepath.Ext(req.Path)
					path := strings.TrimSuffix(filepath.Base(req.Path), ext)
					var split_data [][]byte
					if len(g.spec.Suffix) == 1 {
						split_data = [][]byte{data}
					} else {
						split_data = bytes.Split(data, []byte(g.spec.Separator))
					}
					if len(split_data) > len(g.spec.Suffix) {
						req.Err = fmt.Errorf("too many output. expect equal to or less than %d, got %d: %s", len(g.spec.Suffix), len(split_data), req.Path)
						g.result <- req
						return
					}
					// add output count before sending result
					g.outputCount.Add(int64(len(split_data) - 1))
					for i, suffix := range g.spec.Suffix {
						if i >= len(split_data) {
							break
						}
						path := filepath.Join(g.outputDir, path+suffix)
						go func(path string, data []byte) {
							g.result <- &Result{
								Path: path,
								Data: data,
							}
						}(path, split_data[i])
					}
				}()
			}
		}
	}()
	return nil
}

func (g *Generator) Request(req *Result) {
	go func() {
		g.request <- req
	}()
}

func (g *Generator) askSpec() error {
	cmd := exec.CommandContext(g.ctx, g.generatorPath, "-s")
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
		return errors.New("pass_by is empty")
	}
	if g.spec.Input != "stdin" && g.spec.Input != "file" {
		return errors.New("pass_by must be stdin or file")
	}
	if len(g.spec.Suffix) == 0 {
		return errors.New("suffix is empty")
	}
	if len(g.spec.Suffix) > 1 && len(g.spec.Separator) == 0 {
		return errors.New("separator is empty when suffix is more than 1")
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

func (g *GeneratorHandler) loadAst(path string) ([]byte, error) {
	cmd := exec.CommandContext(g.ctx, g.src2json, path)
	if config.Warnings.DisableUntypedWarning {
		cmd.Args = append(cmd.Args, "--disable-untyped")
	}
	if config.Warnings.DisableUnusedWarning {
		cmd.Args = append(cmd.Args, "--disable-unused")
	}
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

func (g *GeneratorHandler) generateAST(path string, wg *sync.WaitGroup) {
	files, err := g.lookupPath(path)
	if err != nil {
		g.Printf("lookupPath: %s: %s\n", path, err)
		return
	}
	for _, file := range files {
		wg.Add(1)
		go func(file string) {
			defer wg.Done()
			buf, err := g.loadAst(file)
			if err != nil {
				go func() {
					g.errQueue <- fmt.Errorf("loadAst: %s: %w", file, err)
				}()
				return
			}
			for _, gen := range g.generators {
				g.outputCount.Add(1)
				gen.Request(&Result{
					Path: file,
					Data: buf,
				})
			}
		}(file)
	}
}

func (g *GeneratorHandler) dispatchGenerator(out *Output) error {
	gen := NewGenerator(g.ctx, &g.w, g.stderr, g.resultQueue, &g.outputCount)
	err := gen.StartGenerator(out)
	if err != nil {
		return err
	}
	g.generators = append(g.generators, gen)
	return nil
}

func (g *GeneratorHandler) StartGenerator(path ...string) {
	g.queue = make(chan *Result, 1)
	wg := &sync.WaitGroup{}
	for _, p := range path {
		wg.Add(1)
		go func(p string) {
			defer wg.Done()
			g.generateAST(p, wg)
		}(p)
	}

	go func() {
		wg.Wait()
		for {
			if g.outputCount.Load() == 0 {
				close(g.queue)
				g.cancel()
				return
			}
			select {
			case r := <-g.resultQueue:
				g.queue <- r
				g.outputCount.Add(-1)
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

type Output struct {
	Generator string   `json:"generator"`
	OutputDir string   `json:"output_dir"`
	Args      []string `json:"args"`
}

type Warnings struct {
	DisableUntypedWarning bool `json:"disable_untyped"`
	DisableUnusedWarning  bool `json:"disable_unused"`
}

type Config struct {
	Source2Json *string   `json:"src2json"`
	Suffix      *string   `json:"suffix"`
	TargetDirs  []string  `json:"input_dir"`
	Warnings    Warnings  `json:"warnings"`
	Output      []*Output `json:"output"`
}

func loadConfig() (*Config, error) {
	fp, err := os.Open("brgen.json")
	if err != nil {
		if os.IsNotExist(err) {
			return &Config{}, nil // ignore error
		}
		return &Config{}, err
	}
	defer fp.Close()
	var c Config
	err = json.NewDecoder(fp).Decode(&c)
	if err != nil {
		return &Config{}, err
	}
	return &c, nil
}

func fillStringPtr(c *Config) {
	if c.Source2Json == nil {
		c.Source2Json = new(string)
	}
	if c.Suffix == nil {
		c.Suffix = new(string)
		*c.Suffix = ".bgn"
	}
}

var config *Config

func init() {
	var err error
	config, err = loadConfig()
	fillStringPtr(config)
	flag.Func("output", "output information (generator, output_dir)", func(s string) error {
		var o Output
		err := json.Unmarshal([]byte(s), &o)
		if err != nil {
			return err
		}
		config.Output = append(config.Output, &o)
		return nil
	})
	flag.StringVar(config.Source2Json, "src2json", *config.Source2Json, "path to src2json")
	flag.StringVar(config.Suffix, "suffix", *config.Suffix, "suffix of file to generate from")
	flag.BoolVar(&config.Warnings.DisableUntypedWarning, "disable-untyped", config.Warnings.DisableUntypedWarning, "disable untyped warning")
	flag.BoolVar(&config.Warnings.DisableUnusedWarning, "disable-unused", config.Warnings.DisableUnusedWarning, "disable unused warning")

	defer flag.Parse()
	if err != nil {
		log.Print(err)
		return
	}
	if config == nil {
		return
	}
}

func main() {
	log.SetPrefix("brgen: ")
	log.SetOutput(os.Stderr)
	args := flag.Args()
	if len(config.TargetDirs) > 0 {
		args = append(args, config.TargetDirs...)
	}
	if len(args) == 0 {
		flag.Usage()
		return
	}
	g := &GeneratorHandler{}
	g.stderr = os.Stdout
	start := time.Now()
	if err := g.Init(*config.Source2Json, config.Output, *config.Suffix); err != nil {
		log.Fatal(err)
	}
	g.StartGenerator(args...)
	var wg sync.WaitGroup
	var totalCount atomic.Uint32
	var errCount atomic.Uint32
	reporter := func() {
		defer wg.Done()
		for res := range g.Recv() {
			totalCount.Add(1)
			if res.Err != nil {
				g.Printf("%s: %v\n", res.Path, res.Err)
				errCount.Add(1)
				continue
			}
			dir := filepath.Dir(res.Path)
			if dir == "." {
				fmt.Printf("%s:\n%s\n", res.Path, string(res.Data))
				continue
			}
			if err := os.MkdirAll(dir, 0755); err != nil {
				log.Fatal(err)
			}
			fp, err := os.Create(res.Path)
			if err != nil {
				g.Printf("create %s: %s\n", res.Path, err)
				continue
			}
			defer fp.Close()
			_, err = fp.Write(res.Data)
			if err != nil {
				g.Printf("write %s: %s\n", res.Path, err)
				continue
			}
			g.Printf("generated: %s\n", res.Path)
		}
	}
	for i := 0; i < 3; i++ {
		wg.Add(1)
		go reporter()
	}
	wg.Wait()
	elapsed := time.Since(start)
	log.Printf("time: %v total: %d, error: %d\n", elapsed, totalCount.Load(), errCount.Load())
}
