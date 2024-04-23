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

func loadConfig(conf string) (*Config, error) {
	fp, err := os.Open(conf)
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
	if c.LibSource2Json == nil {
		c.LibSource2Json = new(string)
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
	outPuts := []*Output{}
	out := &Output{}
	flag.Func("output", "output dir (both -output and -generator are required to add output config)", func(s string) error {
		if out.OutputDir != "" {
			if out.Generator == "" {
				return errors.New("generator is required")
			}
			outPuts = append(outPuts, out)
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
			outPuts = append(outPuts, out)
			out = &Output{}
		}
		out.Generator = s
		return nil
	})
	flag.String("src2json", "src2json", "path to src2json")
	flag.String("libs2j", "", "path to libs2j")
	flag.String("suffix", ".bgn", "suffix of file to generate from")
	flag.Bool("disable-untyped", false, "disable untyped warning")
	flag.Bool("disable-unused", false, "disable unused warning")
	flag.String("test-info", "", "path to test info output file")
	configLocation := flag.String("config", "brgen.json", "config file location")

	flag.BoolVar(&debug, "debug", false, "debug mode")

	defer func() {
		flag.Parse()
		var err error
		config, err = loadConfig(*configLocation)
		fillStringPtr(config)
		if err != nil {
			log.Print(err)
		}
		config.Output = append(config.Output, outPuts...)
		flag.CommandLine.Visit(func(f *flag.Flag) {
			setString := func(s **string, name string) {
				if f.Name == name {
					*s = new(string)
					**s = f.Value.(flag.Getter).Get().(string)
				}
			}
			setBool := func(b *bool, name string) {
				if f.Name == name {
					*b = f.Value.(flag.Getter).Get().(bool)
				}
			}
			setString(&config.Source2Json, "src2json")
			setString(&config.LibSource2Json, "libs2j")
			setString(&config.Suffix, "suffix")
			setString(&config.TestInfo, "test-info")
			setBool(&config.Warnings.DisableUntypedWarning, "disable-untyped")
			setBool(&config.Warnings.DisableUnusedWarning, "disable-unused")
		})
	}()
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
	if err := g.Init(*config.Source2Json, *config.LibSource2Json, config.Output, *config.Suffix); err != nil {
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
