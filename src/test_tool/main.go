package main

import (
	"encoding/json"
	"log"
	"os"
	"os/exec"
	"sync"
)

type SrcCode []string

func loadFiles(file string, do func(file string)) {
	buf, err := os.ReadFile(file)
	if err != nil {
		log.Fatal(err)
	}
	var code SrcCode
	err = json.Unmarshal(buf, &code)
	if err != nil {
		log.Fatal(err)
	}
	var w sync.WaitGroup
	for _, c := range code {
		w.Add(1)
		go func(code string) {
			defer w.Done()
			file, err := os.CreateTemp(os.TempDir(), "cpp")
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
			do(name)
		}(c)
	}
	w.Wait()
}

func execCpp(name string) {
	cmd := exec.Command("clang++", name)
	cmd.Stderr = os.Stderr
	cmd.Stdout = os.Stdout
	err := cmd.Run()
	if err != nil {
		log.Println(err)
		return
	}
}

var langs map[string]func(string) = map[string]func(string){
	"cpp": execCpp,
}

func main() {
	loadFiles(os.Args[2], langs[os.Args[1]])
}
