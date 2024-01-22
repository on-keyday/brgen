package s2jgo_test

import (
	"io"
	"net/http"
	"os"
	"strings"
	"sync"
	"testing"

	"github.com/on-keyday/brgen/src/tool/s2jgo"
)

func TestLoad(t *testing.T) {
	os.Chdir("C:/workspace/shbrgen/brgen/tool")
	s2j, err := s2jgo.Load("C:/workspace/shbrgen/brgen/tool/libs2j.dll")
	if err != nil {
		t.Fatal(err)
	}
	var s sync.WaitGroup
	s.Add(1)
	go func() {
		err := s2j.CallIOCallback([]string{"src2json", "--via-http"}, s2jgo.CAPABILITY_ALL,
			func(data []byte, isStdErr bool) {
				if isStdErr {
					os.Stderr.Write(data)
				} else {
					os.Stdout.Write(data)
				}
			})
		if err != nil {
			return
		}
		s.Done()
	}()
	r, err := http.Get("http://localhost:8080/stat")
	if err != nil {
		t.Fatal(err)
	}
	io.Copy(os.Stdout, r.Body)
	r.Body.Close()
	r, err = http.Post("http://localhost:8080/parse", "application/json", strings.NewReader(`{"args":["../example/udp.bgn"]}`))
	if err != nil {
		t.Fatal(err)
	}
	io.Copy(os.Stdout, r.Body)
	r.Body.Close()
	r, err = http.Get("http://localhost:8080/stop")
	if err != nil {
		t.Fatal(err)
	}
	io.Copy(os.Stdout, r.Body)
	r.Body.Close()
	s.Wait()

}
