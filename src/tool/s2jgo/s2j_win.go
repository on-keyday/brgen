//go:build windows

package s2jgo

import (
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"unsafe"

	"golang.org/x/sys/windows"
)

func Available() bool {
	return true
}

type Src2JSON struct {
	dll         *windows.DLL
	libs2j_call *windows.Proc
}

func Load(s2j_path string) (_ *Src2JSON, err error) {
	if !filepath.IsAbs(s2j_path) {
		return nil, errors.New("s2j_path must be absolute path")
	}
	path := filepath.Clean(s2j_path)
	// workaround for DLL load
	// change current directory to DLL directory
	cwd, err := os.Getwd()
	if err != nil {
		return nil, err
	}
	err = os.Chdir(filepath.Dir(path))
	if err != nil {
		return nil, err
	}
	defer func() {
		err = os.Chdir(cwd)
	}()
	dll, err := windows.LoadDLL(path)
	if err != nil {
		return nil, err
	}
	proc, err := dll.FindProc("libs2j_call")
	if err != nil {
		dll.Release()
		return nil, err
	}
	runtime.SetFinalizer(dll, func(dll *windows.DLL) {
		dll.Release()
	})
	return &Src2JSON{
		dll:         dll,
		libs2j_call: proc,
	}, nil
}

func (s *Src2JSON) CallIOCallback(args []string, cap Capability, cb func(data []byte, isStdErr bool)) error {
	if len(args) == 0 {
		return errors.New("args must not be empty")
	}
	argh := &argHolder{}
	argc, argv := argh.makeArg(args)
	out_callback := windows.NewCallback(callback)
	data := &outData{
		cb: cb,
	}
	ret, _, err := s.libs2j_call.Call(argc, argv, uintptr(cap), out_callback, uintptr(unsafe.Pointer(data)))
	runtime.KeepAlive(s)
	runtime.KeepAlive(argh)
	runtime.KeepAlive(out_callback)
	runtime.KeepAlive(data)
	if ret != 0 {
		return fmt.Errorf("libs2j_call returned non-zero: %s", strings.TrimSuffix(string(data.stderrCapture), "\n"))
	}
	if err != windows.ERROR_SUCCESS {
		_ = err
	}
	return nil
}
