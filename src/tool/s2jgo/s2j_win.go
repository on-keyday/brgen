//go:build windows

package s2jgo

import (
	"errors"
	"path/filepath"
	"runtime"
	"unsafe"

	"golang.org/x/sys/windows"
)

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

/*
#define S2J_CAPABILITY_STDIN (1 << 0)
#define S2J_CAPABILITY_NETWORK (1 << 1)
#define S2J_CAPABILITY_FILE (1 << 2)
#define S2J_CAPABILITY_ARGV (1 << 3)
#define S2J_CAPABILITY_CHECK_AST (1 << 4)
#define S2J_CAPABILITY_LEXER (1 << 5)
#define S2J_CAPABILITY_PARSER (1 << 6)
#define S2J_CAPABILITY_IMPORTER (1 << 7)
#define S2J_CAPABILITY_AST_JSON (1 << 8)
*/

type Capability uint64

const (
	CAPABILITY_STDIN     Capability = 1 << 0
	CAPABILITY_NETWORK   Capability = 1 << 1
	CAPABILITY_FILE      Capability = 1 << 2
	CAPABILITY_ARGV      Capability = 1 << 3
	CAPABILITY_CHECK_AST Capability = 1 << 4
	CAPABILITY_LEXER     Capability = 1 << 5
	CAPABILITY_PARSER    Capability = 1 << 6
	CAPABILITY_IMPORTER  Capability = 1 << 7
	CAPABILITY_AST_JSON  Capability = 1 << 8

	CAPABILITY_ALL Capability = 0xffffffffffffffff
)

type outData struct {
	stdout []byte
	stderr []byte
}

func callback(data unsafe.Pointer, size uintptr, isStdErr uintptr, ctx unsafe.Pointer) {
	out := (*outData)(ctx)
	if isStdErr != 0 {
		out.stderr = append(out.stderr, unsafe.Slice((*byte)(data), size)...)
	} else {
		out.stdout = append(out.stdout, unsafe.Slice((*byte)(data), size)...)
	}
}

type Result struct {
	Code   int
	Stdout []byte
	Stderr []byte
}

func (s *Src2JSON) Call(args []string, cap Capability) (*Result, error) {
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
	data := &outData{}
	ret, _, err := s.libs2j_call.Call(argc, argv, uintptr(cap), out_callback, uintptr(unsafe.Pointer(data)))
	runtime.KeepAlive(cstrs)
	runtime.KeepAlive(argv_vec)
	runtime.KeepAlive(out_callback)
	runtime.KeepAlive(data)
	if err != nil {
		return nil, err
	}
	return &Result{
		Code:   int(ret),
		Stdout: data.stdout,
		Stderr: data.stderr,
	}, nil
}
