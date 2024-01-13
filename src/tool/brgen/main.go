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
	"net/http"
	"os"
	"os/exec"
	"os/signal"
	gpath "path"
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
	generatorPath     string
	outputDir         string
	args              []string
	spec              Spec
	result            chan *Result
	request           chan *Result
	ctx               context.Context
	stderr            io.Writer
	outputCount       *atomic.Int64
	dirBaseSuffixChan chan DirBaseSuffix
}

type DirBaseSuffix struct {
	Dir    string `json:"dir"`
	Base   string `json:"base"`
	Suffix string `json:"suffix"`
}

type GeneratorHandler struct {
	w         sync.WaitGroup
	src2json  string
	viaHTTP   *exec.Cmd
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

	dirBaseSuffixChan chan DirBaseSuffix
	dirBaseSuffix     []DirBaseSuffix
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
	g.dirBaseSuffixChan = make(chan DirBaseSuffix, 1)
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

func NewGenerator(ctx context.Context, work *sync.WaitGroup, stderr io.Writer, res chan *Result, outputCount *atomic.Int64, dirBaseSuffixChan chan DirBaseSuffix) *Generator {
	return &Generator{
		ctx:               ctx,
		stderr:            stderr,
		request:           make(chan *Result, 1),
		result:            res,
		dirBaseSuffixChan: dirBaseSuffixChan,
		outputCount:       outputCount,
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
					basePath := strings.TrimSuffix(filepath.Base(req.Path), ext)
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
						var path string
						if strings.HasPrefix("/dev/stdout", g.outputDir) {
							baseName := filepath.Base(basePath + suffix)
							path = "/dev/stdout/" + baseName
						} else {
							path = filepath.Join(g.outputDir, basePath+suffix)
						}
						go func(path string, data []byte, suffix string) {
							if runtime.GOOS == "windows" {
								path = strings.ReplaceAll(path, "\\", "/")
							}
							g.dirBaseSuffixChan <- DirBaseSuffix{
								Dir:    gpath.Dir(path),
								Base:   strings.TrimSuffix(gpath.Base(path), suffix),
								Suffix: suffix,
							}
							g.result <- &Result{
								Path: path,
								Data: data,
							}
						}(path, split_data[i], suffix)
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

func (g *GeneratorHandler) appendConfig(args []string) []string {
	if config.Warnings.DisableUntypedWarning {
		args = append(args, "--disable-untyped")
	}
	if config.Warnings.DisableUnusedWarning {
		args = append(args, "--disable-unused")
	}
	return args
}

func (g *GeneratorHandler) loadAst(path string) ([]byte, error) {
	if g.viaHTTP != nil {
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
	gen := NewGenerator(g.ctx, &g.w, g.stderr, g.resultQueue, &g.outputCount, g.dirBaseSuffixChan)
	err := gen.StartGenerator(out)
	if err != nil {
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
		g.requestStop()
		for {
			if g.outputCount.Load() == 0 {
				g.cancel()
				close(g.queue)
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
	TestInfo    *string   `json:"test_info_output"`
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
	if c.TestInfo == nil {
		c.TestInfo = new(string)
	}
}

var config *Config

func init() {
	var err error
	config, err = loadConfig()
	fillStringPtr(config)
	out := &Output{}
	flag.Func("output", "output dir (both -output and -generator are required to add output config)", func(s string) error {
		if out.OutputDir != "" {
			if out.Generator == "" {
				return errors.New("generator is required")
			}
			config.Output = append(config.Output, out)
			out = &Output{}
		}
		out.OutputDir = s
		return nil
	})
	flag.Func("generator", "generator path (both -output and -generator are required to add output config)", func(s string) error {
		if out.Generator != "" {
			if out.OutputDir == "" {
				return errors.New("output is required")
			}
			config.Output = append(config.Output, out)
			out = &Output{}
		}
		out.Generator = s
		return nil
	})
	flag.StringVar(config.Source2Json, "src2json", *config.Source2Json, "path to src2json")
	flag.StringVar(config.Suffix, "suffix", *config.Suffix, "suffix of file to generate from")
	flag.BoolVar(&config.Warnings.DisableUntypedWarning, "disable-untyped", config.Warnings.DisableUntypedWarning, "disable untyped warning")
	flag.BoolVar(&config.Warnings.DisableUnusedWarning, "disable-unused", config.Warnings.DisableUnusedWarning, "disable unused warning")
	flag.StringVar(config.TestInfo, "test-info", *config.TestInfo, "path to test info output file")

	defer func() {
		flag.Parse()
		if out.OutputDir != "" && out.Generator != "" {
			config.Output = append(config.Output, out)
		}
	}()
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
			if strings.HasPrefix(res.Path, "/dev/stdout") {
				p := filepath.Base(res.Path)
				count := strings.Count(string(res.Data), "\n")
				endLine := strings.HasSuffix(string(res.Data), "\n")
				if count < 2 {
					if endLine { // single line
						fmt.Printf("%s: %s", p, string(res.Data))
					} else {
						if count == 0 { // no line break; 1 line
							fmt.Printf("%s: %s\n", p, string(res.Data))
						} else { // 2 lines
							fmt.Printf("%s:\n%s\n", p, string(res.Data))
						}
					}
				} else {
					if endLine { // 3 more lines end with line break
						fmt.Printf("%s:\n%s", p, string(res.Data))
					} else { // 3 more lines end without line break
						fmt.Printf("%s:\n%s\n", p, string(res.Data))
					}
				}
				continue
			}
			dir := filepath.Dir(res.Path)
			if err := os.MkdirAll(dir, 0755); err != nil {
				log.Fatal(err)
			}
			wg.Add(1)
			go func(res *Result) {
				defer wg.Done()
				fp, err := os.Create(res.Path)
				if err != nil {
					g.Printf("create %s: %s\n", res.Path, err)
					return
				}
				defer fp.Close()
				_, err = fp.Write(res.Data)
				if err != nil {
					g.Printf("write %s: %s\n", res.Path, err)
					return
				}
				g.Printf("generated: %s\n", res.Path)
			}(res)
		}
	}
	for i := 0; i < 3; i++ {
		wg.Add(1)
		go reporter()
	}
	wg.Wait()
	elapsed := time.Since(start)
	log.Printf("time: %v total: %d, error: %d\n", elapsed, totalCount.Load(), errCount.Load())
	if *config.TestInfo != "" {
		fp, err := os.Create(*config.TestInfo)
		if err != nil {
			log.Fatal(err)
		}
		defer fp.Close()
		var info struct {
			TotalCount uint32          `json:"total_count"`
			ErrorCount uint32          `json:"error_count"`
			Time       string          `json:"time"`
			DirAndBase []DirBaseSuffix `json:"generated_files"`
		}
		info.TotalCount = totalCount.Load()
		info.ErrorCount = errCount.Load()
		info.Time = elapsed.String()
		info.DirAndBase = g.dirBaseSuffix
		err = json.NewEncoder(fp).Encode(&info)
		if err != nil {
			log.Fatal(err)
		}
	}
}
