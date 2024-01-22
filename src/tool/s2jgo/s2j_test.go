package s2jgo_test

import (
	"testing"

	"github.com/on-keyday/brgen/src/tool/s2jgo"
)

func TestLoad(t *testing.T) {
	s2j, err := s2jgo.Load("C:/workspace/shbrgen/brgen/tool/libs2j.dll")
	if err != nil {
		t.Fatal(err)
	}
	r, err := s2j.Call([]string{"src2json", "--version"}, s2jgo.CAPABILITY_ALL)
	if err != nil {
		t.Fatal(err)
	}
	t.Log(r)
}
