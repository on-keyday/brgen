package main

import (
	"encoding/json"
	"fmt"
	"io/fs"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"sync"
	"sync/atomic"
)

type SrcCode []struct {
	Code string `json:"code"`
}

func removeWithLog(name string) {
	err := os.Remove(name)
	if err != nil {
		log.Println(err)
	}
}

func loadFiles(file, suffix string, do func(file string) error) {
	buf, err := os.ReadFile(file)
	if err != nil {
		log.Fatal(err)
	}
	var code SrcCode
	err = json.Unmarshal(buf, &code)
	if err != nil {
		log.Fatal(err)
	}
	code_dir := filepath.Join(filepath.Dir(file), "/cpp_test_code/")
	var w sync.WaitGroup
	var (
		succeed_count atomic.Uint32
		total_count   atomic.Uint32
	)
	for i, c := range code {
		w.Add(1)
		go func(i int, code string) {
			defer w.Done()
			total_count.Add(1)
			err = os.MkdirAll(code_dir, fs.ModePerm)
			if err != nil {
				log.Print(err)
			}
			err = os.WriteFile(filepath.Join(code_dir, fmt.Sprintf("%s_code%d.%[1]s", suffix, i)), []byte(code), fs.ModePerm)
			if err != nil {
				log.Print(err)
			}
			file, err := os.CreateTemp(os.TempDir(), suffix+"*."+suffix)
			if err != nil {
				log.Print(err)
				return
			}
			_, err = file.WriteString(code)
			if err != nil {
				file.Close()
				log.Print(err)
				return
			}
			name := file.Name()
			file.Close()
			err = do(name)
			removeWithLog(name)
			if err != nil {
				log.Print(err)
				return
			}
			succeed_count.Add(1)
		}(i, c.Code)
	}
	w.Wait()
	suc, total := succeed_count.Load(), total_count.Load()
	log.Printf("%d/%d PASSED", suc, total)
	if suc != total {
		os.Exit(1)
	}
}

func execCpp(name string) error {
	exename := name[:len(name)-4] + ".exe"
	cmd := exec.Command("clang++", name, "-o", exename)
	cmd.Stderr = os.Stderr
	cmd.Stdout = os.Stdout
	err := cmd.Run()
	if err != nil {
		return err
	}
	removeWithLog(exename)
	return err
}

func execGo(name string) error {
	exename := name[:len(name)-3] + ".exe"
	cmd := exec.Command("go", "build", name)
	cmd.Stderr = os.Stderr
	cmd.Stdout = os.Stdout
	err := cmd.Run()
	if err != nil {
		return err
	}
	removeWithLog(exename)
	return err
}

var langs map[string]func(string) error = map[string]func(string) error{
	"cpp": execCpp,
	"go":  execGo,
}

func main() {
	suffix := os.Args[1]
	loadFiles(os.Args[2], suffix, langs[suffix])
}
