package main

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io/fs"
	"log"
	"os"
	"os/exec"
	"os/signal"
	"path/filepath"
	"runtime"
	"strings"
	"sync"
)

type Spec struct {
	Langs  []string `json:"langs"`
	PassBy string   `json:"pass_by"`
	Suffix string   `json:"suffix"`
}

type Result struct {
	Path string
	Data []byte
}

type GeneratorHandler struct {
	w             sync.WaitGroup
	src2json      string
	json2code     string
	ctx           context.Context
	cancel        context.CancelFunc
	spec          Spec
	tmpQueue      chan *Result
	queue         chan *Result
	suffixPattern string
}

func (g *GeneratorHandler) Init(src2json string, json2code string, suffix string) error {
	if json2code == "" {
		return errors.New("json2code is required")
	}
	g.json2code = json2code
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
	if err := g.askSpec(); err != nil {
		return fmt.Errorf("askSpec: %w", err)
	}
	g.suffixPattern = suffix
	return nil
}

func (g *GeneratorHandler) askSpec() error {
	cmd := exec.CommandContext(g.ctx, g.json2code, "-s")
	cmd.Stderr = os.Stderr
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
	cmd.Stderr = os.Stderr
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

func (g *GeneratorHandler) execGenerator(cmd *exec.Cmd) ([]byte, error) {
	cmd.Stderr = os.Stderr
	buf := bytes.NewBuffer(nil)
	cmd.Stdout = buf
	err := cmd.Run()
	if err != nil {
		return nil, err
	}
	return buf.Bytes(), nil
}

func (g *GeneratorHandler) passAst(buffer []byte) ([]byte, error) {
	if g.spec.PassBy == "stdin" {
		cmd := exec.CommandContext(g.ctx, g.json2code)
		cmd.Stdin = bytes.NewReader(buffer)
		return g.execGenerator(cmd)
	}
	path, err := makeTmpFile(buffer)
	if err != nil {
		return nil, err
	}
	defer os.Remove(path)
	cmd := exec.CommandContext(g.ctx, g.json2code, path)
	return g.execGenerator(cmd)
}

func (g *GeneratorHandler) generate(path string) {
	files, err := g.lookupPath(path)
	if err != nil {
		log.Printf("lookupPath: %s: %s\n", path, err)
		return
	}
	var wg sync.WaitGroup
	for _, file := range files {
		wg.Add(1)
		go func(file string) {
			defer wg.Done()
			log.Printf("start: %s\n", file)
			defer log.Printf("done: %s\n", file)
			buf, err := g.loadAst(file)
			if err != nil {
				log.Printf("loadAst: %s: %s\n", file, err)
				return
			}
			buf, err = g.passAst(buf)
			if err != nil {
				log.Printf("passAst: %s: %s\n", file, err)
				return
			}
			g.tmpQueue <- &Result{
				Path: file,
				Data: buf,
			}
		}(file)
	}
	wg.Wait()
}

func (g *GeneratorHandler) dispatchGenerator(path string) {
	g.w.Add(1)
	go func() {
		defer g.w.Done()
		g.generate(path)
	}()
}

func (g *GeneratorHandler) StartGenerator(path ...string) {
	g.tmpQueue = make(chan *Result, 1)
	for i, p := range path {
		if i == len(path)-1 {
			g.dispatchGenerator(p)
			continue
		}
		g.dispatchGenerator(p)
	}
	go func() {
		g.w.Wait()
		close(g.tmpQueue)
	}()
	go func() {
		que := []*Result{}
	lab:
		for {
			select {
			case <-g.ctx.Done():
				close(g.queue)
				return
			case res, ok := <-g.tmpQueue:
				if !ok {
					break lab
				}
				que = append(que, res)
			default:
			}
		}
		for _, v := range que {
			g.queue <- v
		}
		close(g.queue)
	}()
}

func (g *GeneratorHandler) Recv() <-chan *Result {
	if g.queue == nil {
		g.queue = make(chan *Result, 1)
	}
	return g.queue
}

type Config struct {
	Source2Json   *string  `json:"src2json"`
	Json2Code     *string  `json:"json2code"`
	SuffixPattern *string  `json:"suffix_pattern"`
	Targets       []string `json:"targets"`
	OutputDir     *string  `json:"output_dir"`
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
	if c.Json2Code == nil {
		c.Json2Code = new(string)
	}
	if c.Source2Json == nil {
		c.Source2Json = new(string)
	}
	if c.SuffixPattern == nil {
		c.SuffixPattern = new(string)
		*c.SuffixPattern = ".bgn"
	}
	if c.OutputDir == nil {
		c.OutputDir = new(string)
	}
}

var config *Config

func init() {
	var err error
	config, err = loadConfig()
	fillStringPtr(config)
	flag.StringVar(config.Json2Code, "json2code", *config.Json2Code, "path to json2code")
	flag.StringVar(config.Source2Json, "src2json", *config.Source2Json, "path to src2json")
	flag.StringVar(config.Json2Code, "G", *config.Json2Code, "alias of json2code")
	flag.StringVar(config.SuffixPattern, "suffix", *config.SuffixPattern, "suffix of file to generate from")
	flag.StringVar(config.OutputDir, "o", *config.OutputDir, "output directory")

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
	if len(config.Targets) > 0 {
		args = append(args, config.Targets...)
	}
	if len(args) == 0 {
		flag.Usage()
		return
	}
	g := &GeneratorHandler{}
	if err := g.Init(*config.Source2Json, *config.Json2Code, *config.SuffixPattern); err != nil {
		log.Fatal(err)
	}
	g.StartGenerator(args...)
	first := true
	for res := range g.Recv() {
		if *config.OutputDir == "" {
			fmt.Printf("%s:\n", res.Path)
			fmt.Println(string(res.Data))
			continue
		}
		path := strings.TrimSuffix(filepath.Base(res.Path), *config.SuffixPattern)
		path = filepath.Join(*config.OutputDir, path+g.spec.Suffix)
		if first {
			if err := os.MkdirAll(*config.OutputDir, 0755); err != nil {
				log.Fatal(err)
			}
			first = false
		}
		fp, err := os.Create(path)
		if err != nil {
			log.Printf("create %s: %s\n", path, err)
			continue
		}
		defer fp.Close()
		_, err = fp.Write(res.Data)
		if err != nil {
			log.Printf("write %s: %s\n", path, err)
			continue
		}
		log.Printf("generated: %s\n", path)
	}
}
