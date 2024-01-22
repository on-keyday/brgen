package s2jgo

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
