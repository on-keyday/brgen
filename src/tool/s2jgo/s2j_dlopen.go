//go:build linux || darwin || freebsd || openbsd || netbsd || dragonfly || solaris

package s2jgo

/*
#cgo LDFLAGS: -ldl
#include <dlfcn.h>
#include <stddef.h>
#include <stdint.h>

extern void s2jgo_callback(char* str, size_t len, size_t is_stderr, void* data);
typedef int(*s2jgo_call_t)(int, void*, uint64_t,void(*)(char*,size_t,size_t,void*), void*);

int call_s2j_pointer(void* proc_ptr,int argc,void* argv,uint64_t cap,void* data){
    s2jgo_call_t proc=(s2jgo_call_t)proc_ptr;
	return proc(argc,argv,cap,s2jgo_callback,data);
}
*/
import "C"
import (
	"errors"
	"fmt"
	"path/filepath"
	"runtime"
	"strings"
	"unsafe"
)

//export s2jgo_callback
func s2jgo_callback(str *C.char, len C.size_t, is_stderr C.size_t, data unsafe.Pointer) {
	callback(unsafe.Pointer(str), uintptr(len), uintptr(is_stderr), data)
}

func Available() bool {
	return true
}

type Src2JSON struct {
	dll  unsafe.Pointer
	proc unsafe.Pointer
}

func Load(path string) (*Src2JSON, error) {
	if !filepath.IsAbs(path) {
		return nil, errors.New("s2j_path must be absolute path")
	}
	dll := C.dlopen(C.CString(path), C.RTLD_LAZY)
	if dll == nil {
		return nil, fmt.Errorf("failed to load %s", path)
	}
	proc := C.dlsym(dll, C.CString("libs2j_call"))
	if proc == nil {
		C.dlclose(dll)
		return nil, fmt.Errorf("failed to find Src2JSON in %s", path)
	}
	runtime.SetFinalizer(dll, func(dll *C.void) {
		C.dlclose(dll)
	})
	return &Src2JSON{
		dll:  unsafe.Pointer(dll),
		proc: unsafe.Pointer(proc),
	}, nil
}

func (s *Src2JSON) CallIOCallback(args []string, cap Capability, cb func(data []byte, isStdErr bool)) error {
	if len(args) == 0 {
		return errors.New("args must not be empty")
	}
	argh := &argHolder{}
	argc, argv := argh.makeArg(args)
	out := &outData{cb: cb}
	ret := C.call_s2j_pointer((*C.void)s.proc, 
		C.int(argc), 
		(*C.void)unsafe.Pointer(argv), 
		C.uint64_t(cap),
		(*C.void)unsafe.Pointer(out),
	)
	runtime.KeepAlive(s)
	runtime.KeepAlive(argh)
	runtime.KeepAlive(out)
	if ret != 0 {
		return fmt.Errorf("libs2j_call returned non-zero: %s", strings.TrimSuffix(string(out.stderrCapture), "\n"))
	}
	return nil
}
