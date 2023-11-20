package tool_test

import (
	"bytes"
	"context"
	"errors"
	"os/exec"
	"runtime"
	"testing"
	"time"
)

const src2jsonPath = "../../../tool/src2json"

func FuzzSrc2JSON(f *testing.F) {
	exeName := src2jsonPath
	if runtime.GOOS == "windows" {
		exeName += ".exe"
	}
	f.Fuzz(func(t *testing.T, data []byte) {
		c, cancel := context.WithTimeoutCause(context.Background(), 5*time.Second, errors.New("process timeout"))
		defer cancel()
		cmd := exec.CommandContext(c, exeName, "--stdin")
		pipe, err := cmd.StdinPipe()
		if err != nil {
			t.Fatal(err)
		}

		var stdout, stderr bytes.Buffer
		cmd.Stdout = &stdout
		cmd.Stderr = &stderr

		err = cmd.Start()
		if err != nil {
			t.Fatal(err)
		}
		_, err = pipe.Write(data)
		if err != nil {
			t.Fatal(err)
		}
		err = pipe.Close()
		if err != nil {
			t.Fatal(err)
		}
		err = cmd.Wait()
		t.Log("stdout:", stdout.String())
		t.Log("stderr:", stderr.String())
		if err != nil {
			if exitErr, ok := err.(*exec.ExitError); ok {
				if exitErr.ExitCode() == 0 || exitErr.ExitCode() == 1 {
					err = nil
				}
			}
			if err != nil {
				t.Fatal(err)
			}
		}

	})
}
