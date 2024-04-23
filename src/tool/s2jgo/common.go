package s2jgo

import (
	"runtime"
	"unsafe"
)

type Result struct {
	Stdout []byte
	Stderr []byte
}

type argHolder struct {
	argvVec []uintptr
	cStrs   [][]byte
	runtime.Pinner
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

func (a *argHolder) makeArg(args []string) (argc uintptr, argv uintptr) {
	a.argvVec = make([]uintptr, len(args)+1)
	a.cStrs = make([][]byte, len(args))
	for i, arg := range args {
		a.cStrs[i] = append([]byte(arg), 0)
	}
	for i := range a.cStrs {
		data := unsafe.SliceData(a.cStrs[i])
		a.Pin(data)
		a.argvVec[i] = uintptr(unsafe.Pointer(data))
	}
	a.argvVec[len(args)] = 0
	argc = uintptr(len(args))
	data := unsafe.SliceData(a.argvVec)
	a.Pin(data)
	argv = uintptr(unsafe.Pointer(data))
	return
}

func Load(path string) (*Src2JSON, error) {
	s, err := load(path)
	if err != nil {
		return nil, err
	}
	return &Src2JSON{s}, nil

}

type Src2JSON struct {
	*src2JSON
}

func (s *Src2JSON) Call(args []string, cap Capability) (*Result, error) {
	var stdout, stderr []byte
	err := s.src2JSON.CallIOCallback(args, cap, func(data []byte, isStdErr bool) {
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
