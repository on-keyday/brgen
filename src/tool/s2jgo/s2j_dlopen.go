//go:build cgo && (linux || darwin || freebsd)

package s2jgo

/*
#cgo LDFLAGS: -ldl
#include <dlfcn.h>
#include <stddef.h>
#include <stdint.h>

extern void s2jgo_callback(char* str, size_t len, size_t is_stderr, void* data);
typedef int(*s2jgo_call_t)(int, void*, uint64_t,void(*)(char*,size_t,size_t,void*), void*);

inline int call_s2j_pointer(void* proc_ptr,int argc,uintptr_t argv,uint64_t cap,uintptr_t data){
    s2jgo_call_t proc=(s2jgo_call_t)proc_ptr;
	return proc(argc,(void*)argv,cap,s2jgo_callback,(void*)data);
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

type src2JSON struct {
	dll  unsafe.Pointer
	proc unsafe.Pointer
}

func load(path string) (*src2JSON, error) {
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
		return nil, fmt.Errorf("failed to find libs2j_call in %s", path)
	}
	s2j := &src2JSON{
		dll:  unsafe.Pointer(dll),
		proc: unsafe.Pointer(proc),
	}
	runtime.SetFinalizer(s2j, func(dll *src2JSON) {
		C.dlclose(dll.dll)
	})
	return s2j, nil
}

func (s *src2JSON) CallIOCallback(args []string, cap Capability, cb func(data []byte, isStdErr bool)) error {
	if len(args) == 0 {
		return errors.New("args must not be empty")
	}
	argh := &argHolder{}
	argc, argv := argh.makeArg(args)
	data := &outData{cb: cb}
	argh.Pin(data)
	defer argh.Unpin()

	ptr := uintptr(unsafe.Pointer(data))

	ret := C.call_s2j_pointer(s.proc,
		(C.int)(argc),
		(C.uintptr_t)(argv),
		(C.uint64_t)(cap),
		(C.uintptr_t)(ptr),
	)
	runtime.KeepAlive(s)
	runtime.KeepAlive(argh)
	runtime.KeepAlive(data)
	if ret != 0 {
		return fmt.Errorf("libs2j_call returned non-zero: %s", strings.TrimSuffix(string(data.stderrCapture), "\n"))
	}
	return nil
}
