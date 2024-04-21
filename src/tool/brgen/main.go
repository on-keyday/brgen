package main

import (
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io"
	"log"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"sync/atomic"
	"time"
)

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

var debug bool

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

	flag.BoolVar(&debug, "debug", false, "debug mode")

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

var exitCode int

func main() {
	defer os.Exit(exitCode)
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
	if debug {
		g.stderr = os.Stdout
	} else {
		g.stderr = io.Discard
	}
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
			TotalCount uint32           `json:"total_count"`
			ErrorCount uint32           `json:"error_count"`
			Time       string           `json:"time"`
			DirAndBase []*DirBaseSuffix `json:"generated_files"`
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
	if errCount.Load() > 0 {
		exitCode = 1
	}
}
