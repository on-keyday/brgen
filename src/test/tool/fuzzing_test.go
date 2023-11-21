package tool_test

import (
	"bytes"
	"context"
	"errors"
	"fmt"
	"os"
	"os/exec"
	"runtime"
	"syscall"
	"testing"
	"time"

	"golang.org/x/net/websocket"
)

const src2jsonPath = "../../../tool/src2json"

func FuzzSrc2JSON(f *testing.F) {
	wsConn, err := websocket.Dial("ws://localhost:8080/log", "", "http://localhost/")
	if err != nil {
		f.Fatal(err)
	}
	defer wsConn.Close()

	exeName := src2jsonPath
	if runtime.GOOS == "windows" {
		exeName += ".exe"
	}
	data := []string{
		"format", "if", "elif", "else", "match", "fn", "loop", "enum",
		"input", "output", "config", "true", "false",
		"return", "break", "continue", "cast",
		"#", "\"", "'", "$", // added but maybe not used
		"::=", ":=",
		":", ";", "(", ")", "[", "]",
		"=>", "==", "=",
		"..=", "..", ".", "->",
		"+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>=", ">>>=", "<<<=",
		">>>", "<<<", ">>", "<<", "~",
		"&&", "||", "&", "|",
		"!=", "!",
		"+", "-", "*", "/", "%", "^",
		"<=", ">=", "<", ">", "?", ",",
	}
	for _, d := range data {
		f.Add([]byte(d))
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
		wsConn.Write([]byte(fmt.Sprintf("stdout: %s\nstderr: %s\ndata: %q\n", stdout.String(), stderr.String(), data)))
		t.Log("stdout:", stdout.String())
		t.Log("stderr:", stderr.String())
		if err != nil {
			if exitErr, ok := err.(*exec.ExitError); ok {
				if exitErr.ExitCode() == 0 || exitErr.ExitCode() == 1 {
					err = nil
				}
			}
			if w := errors.Unwrap(err); w != nil {
				if syscallErr, ok := w.(*os.SyscallError); ok {
					if syscallErr.Syscall == "TerminateProcess" &&
						syscallErr.Err == syscall.ERROR_ACCESS_DENIED {
						err = nil
					}
				}
			}
			if err != nil {
				t.Logf("err: %#v", err)
				t.Fatal(err)
			}
		}

	})
}
