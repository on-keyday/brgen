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
}

type Generator struct {
	w         sync.WaitGroup
	src2json  string
	json2code string
	ctx       context.Context
	cancel    context.CancelFunc
	spec      Spec
}

func (g *Generator) Init(src2json string, json2code string) error {
	if json2code == "" {
		return errors.New("json2code is required")
	}
	g.json2code = json2code
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
	g.ctx, g.cancel = signal.NotifyContext(context.Background(), os.Interrupt)
	if err := g.askSpec(); err != nil {
		return err
	}
	return nil
}

func (g *Generator) askSpec() error {
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

func (g *Generator) execGenerator(cmd *exec.Cmd) ([]byte, error) {
	cmd.Stderr = os.Stderr
	buf := bytes.NewBuffer(nil)
	cmd.Stdout = buf
	err := cmd.Run()
	if err != nil {
		return nil, err
	}
	return buf.Bytes(), nil
}

func (g *Generator) passAst(buffer []byte) ([]byte, error) {
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

func (g *Generator) generate(path string) {
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
			fmt.Printf("%s\n", buf)
		}(file)
	}
	wg.Wait()
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
var suffix = flag.String("suffix", ".bgn", "suffix of file to generate from")

type Config struct {
	Source2Json *string `json:"src2json"`
	Json2Code   *string `json:"json2code"`
	Suffix      *string `json:"suffix"`
}

func loadConfig() (*Config, error) {
	fp, err := os.Open("brgen.json")
	if err != nil {
		if os.IsNotExist(err) {
			return nil, nil // ignore error
		}
		return nil, err
	}
	defer fp.Close()
	var c Config
	err = json.NewDecoder(fp).Decode(&c)
	if err != nil {
		return nil, err
	}
	return &c, nil
}

func init() {
	flag.StringVar(json2code, "json2code", "", "path to json2code")
	config, err := loadConfig()
	defer flag.Parse()
	if err != nil {
		log.Print(err)
		return
	}
	if config == nil {
		return
	}
	if config.Source2Json != nil {
		*src2json = *config.Source2Json
	}
	if config.Json2Code != nil {
		*json2code = *config.Json2Code
	}
	if config.Suffix != nil {
		*suffix = *config.Suffix
	}
}

func main() {
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
