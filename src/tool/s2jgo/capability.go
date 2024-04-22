package s2jgo

import (
	"unsafe"
)

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

type Result struct {
	Stdout []byte
	Stderr []byte
}

type argHolder struct {
	argvVec []uintptr
	cStrs   [][]byte
}

func (a *argHolder) makeArg(args []string) (argc uintptr, argv uintptr) {
	a.argvVec = make([]uintptr, len(args)+1)
	a.cStrs = make([][]byte, len(args))
	for i, arg := range args {
		a.cStrs[i] = append([]byte(arg), 0)
	}
	for i := range a.cStrs {
		a.argvVec[i] = uintptr(unsafe.Pointer(unsafe.SliceData(a.cStrs[i])))
	}
	a.argvVec[len(args)] = 0
	argc = uintptr(len(args))
	argv = uintptr(unsafe.Pointer(unsafe.SliceData(a.argvVec)))
	return
}
