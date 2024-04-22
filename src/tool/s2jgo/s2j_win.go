//go:build windows

package s2jgo

import (
	"errors"
	"fmt"
	"path/filepath"
	"runtime"
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

func Load(s2j_path string) (*Src2JSON, error) {
	if !filepath.IsAbs(s2j_path) {
		return nil, errors.New("s2j_path must be absolute path")
	}
	path := filepath.Clean(s2j_path)
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

type outData struct {
	cb            func(data []byte, isStdErr bool)
	stderrCapture []byte
}

func callback(data unsafe.Pointer, size uintptr, isStdErr uintptr, ctx unsafe.Pointer) uintptr {
	out := (*outData)(ctx)
	if out.cb != nil {
		out.cb(unsafe.Slice((*byte)(data), size), isStdErr != 0)
	}
	if isStdErr != 0 {
		out.stderrCapture = append(out.stderrCapture, unsafe.Slice((*byte)(data), size)...)
	}
	return 0
}


func (s *Src2JSON) Call(args []string, cap Capability) (*Result, error) {
	var stdout, stderr []byte
	err := s.CallIOCallback(args, cap, func(data []byte, isStdErr bool) {
		if isStdErr {
			stderr = append(stderr, data...)
		} else {
			stdout = append(stdout, data...)
		}
	})
	if err != nil {
		return nil, err
	}
	return &Result{
		Stdout: stdout,
		Stderr: stderr,
	}, nil
}

func (s *Src2JSON) CallIOCallback(args []string, cap Capability, cb func(data []byte, isStdErr bool)) error {
	if len(args) == 0 {
		return errors.New("args must not be empty")
	}
	argv_vec := make([]uintptr, len(args)+1)
	cstrs := make([][]byte, len(args))
	for i, arg := range args {
		cstrs[i] = append([]byte(arg), 0)
		argv_vec[i] = uintptr(unsafe.Pointer(unsafe.SliceData(cstrs[i])))
	}
	argv_vec[len(args)] = 0
	argc := uintptr(len(args))
	argv := uintptr(unsafe.Pointer(unsafe.SliceData(argv_vec)))
	out_callback := windows.NewCallback(callback)
	data := &outData{
		cb: cb,
	}
	ret, _, err := s.libs2j_call.Call(argc, argv, uintptr(cap), out_callback, uintptr(unsafe.Pointer(data)))
	runtime.KeepAlive(cstrs)
	runtime.KeepAlive(argv_vec)
	runtime.KeepAlive(out_callback)
	runtime.KeepAlive(data)
	if err != nil && err != windows.ERROR_SUCCESS {
		return err
	}
	if ret != 0 {
		return fmt.Errorf("libs2j_call returned non-zero: %s", string(data.stderrCapture))
	}
	return nil
}
