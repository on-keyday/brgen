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
)

type Spec struct {
	Langs  []string `json:"langs"`
	PassBy string   `json:"pass_by"`
	Suffix string   `json:"suffix"`
}

type Result struct {
	Path string
	Data []byte
	Err  error
}

type Generator struct {
	generatorPath string
	outputDir     string
	spec          Spec
	result        chan *Result
	request       chan *Result
	ctx           context.Context
	stderr        io.Writer
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
	outputCount   atomic.Int32
}

func (g *GeneratorHandler) Init(src2json string, output []*Output, suffix string) error {
	if len(output) == 0 {
		return errors.New("output is required")
	}
	if src2json != "" {
		g.src2json = src2json
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

func NewGenerator(ctx context.Context, work *sync.WaitGroup, stderr io.Writer, res chan *Result) *Generator {
	return &Generator{
		ctx:     ctx,
		stderr:  stderr,
		request: make(chan *Result, 1),
		result:  res,
	}
}

func (g *Generator) execGenerator(cmd *exec.Cmd) ([]byte, error) {
	cmd.Stderr = g.stderr
	buf := bytes.NewBuffer(nil)
	cmd.Stdout = buf
	err := cmd.Run()
	if err != nil {
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

func (g *Generator) passAst(buffer []byte) ([]byte, error) {
	if g.spec.PassBy == "stdin" {
		cmd := exec.CommandContext(g.ctx, g.generatorPath)
		cmd.Stdin = bytes.NewReader(buffer)
		return g.execGenerator(cmd)
	}
	path, err := makeTmpFile(buffer)
	if err != nil {
		return nil, err
	}
	defer os.Remove(path)
	cmd := exec.CommandContext(g.ctx, g.generatorPath, path)
	return g.execGenerator(cmd)
}

func (g *Generator) StartGenerator(out *Output) error {
	g.generatorPath = out.Generator
	g.outputDir = out.OutputDir
	err := g.askSpec()
	if err != nil {
		return fmt.Errorf("%s: askSpec: %w", g.generatorPath, err)
	}
	go func() {
		for {
			select {
			case <-g.ctx.Done():
				return
			case req := <-g.request:
				go func() {
					data, err := g.passAst(req.Data)
					if err != nil {
						req.Err = err
						g.result <- req
						return
					}
					ext := filepath.Ext(req.Path)
					path := strings.TrimSuffix(filepath.Base(req.Path), ext)
					path = filepath.Join(g.outputDir, path+g.spec.Suffix)
					go func() {
						g.result <- &Result{
							Path: path,
							Data: data,
						}
					}()
				}()
			}
		}
	}()
	return nil
}

func (g *Generator) Request(req *Result) {
	g.request <- req
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
	if g.spec.PassBy == "" {
		return errors.New("pass_by is empty")
	}
	if g.spec.PassBy != "stdin" && g.spec.PassBy != "file" {
		return errors.New("pass_by must be stdin or file")
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
	if *config.DisableUntypedWarning {
		cmd.Args = append(cmd.Args, "--disable-untyped")
	}
	cmd.Stderr = g.stderr
	buf := bytes.NewBuffer(nil)
	cmd.Stdout = buf
	err := cmd.Run()
	if err != nil {
		return nil, err
	}
	return buf.Bytes(), nil
}

func (g *GeneratorHandler) generateAST(path string) {
	files, err := g.lookupPath(path)
	if err != nil {
		log.Printf("lookupPath: %s: %s\n", path, err)
		return
	}
	for _, file := range files {
		go func(file string) {
			log.Printf("start: %s\n", file)
			defer log.Printf("done: %s\n", file)
			buf, err := g.loadAst(file)
			if err != nil {
				g.errQueue <- fmt.Errorf("loadAst: %s: %w\n", file, err)
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
	gen := NewGenerator(g.ctx, &g.w, g.stderr, g.resultQueue)
	err := gen.StartGenerator(out)
	if err != nil {
		return err
	}
	g.generators = append(g.generators, gen)
	return nil
}

func (g *GeneratorHandler) StartGenerator(path ...string) {
	for _, p := range path {
		go func(p string) {
			g.generateAST(p)
		}(p)
	}
	g.queue = make(chan *Result, 1)
	go func() {
		for {
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
			if g.outputCount.Load() == 0 {
				close(g.queue)
				g.cancel()
				return
			}
		}
	}()
}

func (g *GeneratorHandler) Recv() <-chan *Result {
	return g.queue
}

type Output struct {
	Generator string `json:"generator"`
	OutputDir string `json:"output_dir"`
}

type Config struct {
	Source2Json           *string  `json:"src2json"`
	SuffixPattern         *string  `json:"suffix_pattern"`
	TargetDirs            []string `json:"input_dir"`
	DisableUntypedWarning *bool    `json:"disable_untyped_warning"`
	Output                []*Output
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
	if c.SuffixPattern == nil {
		c.SuffixPattern = new(string)
		*c.SuffixPattern = ".bgn"
	}
	if c.DisableUntypedWarning == nil {
		c.DisableUntypedWarning = new(bool)
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
	flag.StringVar(config.SuffixPattern, "suffix", *config.SuffixPattern, "suffix of file to generate from")
	flag.BoolVar(config.DisableUntypedWarning, "disable-untyped", *config.DisableUntypedWarning, "disable untyped warning")

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
	if err := g.Init(*config.Source2Json, config.Output, *config.SuffixPattern); err != nil {
		log.Fatal(err)
	}
	g.StartGenerator(args...)

	for res := range g.Recv() {
		if res.Err != nil {
			log.Printf("%s: %v", res.Path, res.Err)
			continue
		}
		dir := filepath.Dir(res.Path)
		if dir == "." {
			fmt.Printf("%s:\n", res.Path)
			fmt.Println(string(res.Data))
			continue
		}
		if err := os.MkdirAll(dir, 0755); err != nil {
			log.Fatal(err)
		}
		fp, err := os.Create(res.Path)
		if err != nil {
			log.Printf("create %s: %s\n", res.Path, err)
			continue
		}
		defer fp.Close()
		_, err = fp.Write(res.Data)
		if err != nil {
			log.Printf("write %s: %s\n", res.Path, err)
			continue
		}
		log.Printf("generated: %s\n", res.Path)
	}
}
