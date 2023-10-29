// Code generated by gen_ast2c.go. DO NOT EDIT.

#include "ast.h"
#include<string.h>

#ifdef __cplusplus
extern "C" {
#else
#include<stdalign.h>
#endif

int NodeType_from_string(const char* str, NodeType* out) {
	if (strcmp("node", str) == 0) {
		*out = NODE;
		return 1;
	}
	if (strcmp("program", str) == 0) {
		*out = PROGRAM;
		return 1;
	}
	if (strcmp("expr", str) == 0) {
		*out = EXPR;
		return 1;
	}
	if (strcmp("binary", str) == 0) {
		*out = BINARY;
		return 1;
	}
	if (strcmp("unary", str) == 0) {
		*out = UNARY;
		return 1;
	}
	if (strcmp("cond", str) == 0) {
		*out = COND;
		return 1;
	}
	if (strcmp("ident", str) == 0) {
		*out = IDENT;
		return 1;
	}
	if (strcmp("call", str) == 0) {
		*out = CALL;
		return 1;
	}
	if (strcmp("if", str) == 0) {
		*out = IF;
		return 1;
	}
	if (strcmp("member_access", str) == 0) {
		*out = MEMBER_ACCESS;
		return 1;
	}
	if (strcmp("paren", str) == 0) {
		*out = PAREN;
		return 1;
	}
	if (strcmp("index", str) == 0) {
		*out = INDEX;
		return 1;
	}
	if (strcmp("match", str) == 0) {
		*out = MATCH;
		return 1;
	}
	if (strcmp("range", str) == 0) {
		*out = RANGE;
		return 1;
	}
	if (strcmp("tmp_var", str) == 0) {
		*out = TMP_VAR;
		return 1;
	}
	if (strcmp("block_expr", str) == 0) {
		*out = BLOCK_EXPR;
		return 1;
	}
	if (strcmp("import", str) == 0) {
		*out = IMPORT;
		return 1;
	}
	if (strcmp("literal", str) == 0) {
		*out = LITERAL;
		return 1;
	}
	if (strcmp("int_literal", str) == 0) {
		*out = INT_LITERAL;
		return 1;
	}
	if (strcmp("bool_literal", str) == 0) {
		*out = BOOL_LITERAL;
		return 1;
	}
	if (strcmp("str_literal", str) == 0) {
		*out = STR_LITERAL;
		return 1;
	}
	if (strcmp("input", str) == 0) {
		*out = INPUT;
		return 1;
	}
	if (strcmp("output", str) == 0) {
		*out = OUTPUT;
		return 1;
	}
	if (strcmp("config", str) == 0) {
		*out = CONFIG;
		return 1;
	}
	if (strcmp("stmt", str) == 0) {
		*out = STMT;
		return 1;
	}
	if (strcmp("loop", str) == 0) {
		*out = LOOP;
		return 1;
	}
	if (strcmp("indent_block", str) == 0) {
		*out = INDENT_BLOCK;
		return 1;
	}
	if (strcmp("match_branch", str) == 0) {
		*out = MATCH_BRANCH;
		return 1;
	}
	if (strcmp("return", str) == 0) {
		*out = RETURN;
		return 1;
	}
	if (strcmp("break", str) == 0) {
		*out = BREAK;
		return 1;
	}
	if (strcmp("continue", str) == 0) {
		*out = CONTINUE;
		return 1;
	}
	if (strcmp("assert", str) == 0) {
		*out = ASSERT;
		return 1;
	}
	if (strcmp("implicit_yield", str) == 0) {
		*out = IMPLICIT_YIELD;
		return 1;
	}
	if (strcmp("member", str) == 0) {
		*out = MEMBER;
		return 1;
	}
	if (strcmp("field", str) == 0) {
		*out = FIELD;
		return 1;
	}
	if (strcmp("format", str) == 0) {
		*out = FORMAT;
		return 1;
	}
	if (strcmp("function", str) == 0) {
		*out = FUNCTION;
		return 1;
	}
	if (strcmp("type", str) == 0) {
		*out = TYPE;
		return 1;
	}
	if (strcmp("int_type", str) == 0) {
		*out = INT_TYPE;
		return 1;
	}
	if (strcmp("ident_type", str) == 0) {
		*out = IDENT_TYPE;
		return 1;
	}
	if (strcmp("int_literal_type", str) == 0) {
		*out = INT_LITERAL_TYPE;
		return 1;
	}
	if (strcmp("str_literal_type", str) == 0) {
		*out = STR_LITERAL_TYPE;
		return 1;
	}
	if (strcmp("void_type", str) == 0) {
		*out = VOID_TYPE;
		return 1;
	}
	if (strcmp("bool_type", str) == 0) {
		*out = BOOL_TYPE;
		return 1;
	}
	if (strcmp("array_type", str) == 0) {
		*out = ARRAY_TYPE;
		return 1;
	}
	if (strcmp("function_type", str) == 0) {
		*out = FUNCTION_TYPE;
		return 1;
	}
	if (strcmp("struct_type", str) == 0) {
		*out = STRUCT_TYPE;
		return 1;
	}
	if (strcmp("union_type", str) == 0) {
		*out = UNION_TYPE;
		return 1;
	}
	if (strcmp("cast", str) == 0) {
		*out = CAST;
		return 1;
	}
	if (strcmp("comment", str) == 0) {
		*out = COMMENT;
		return 1;
	}
	if (strcmp("comment_group", str) == 0) {
		*out = COMMENT_GROUP;
		return 1;
	}
	if (strcmp("union_field", str) == 0) {
		*out = UNION_FIELD;
		return 1;
	}
	if (strcmp("union_candidate", str) == 0) {
		*out = UNION_CANDIDATE;
		return 1;
	}
	return 0;
}

const char* NodeType_to_string(NodeType typ) {
	if (typ == NODE) {
		return "node";
	}
	if (typ == PROGRAM) {
		return "program";
	}
	if (typ == EXPR) {
		return "expr";
	}
	if (typ == BINARY) {
		return "binary";
	}
	if (typ == UNARY) {
		return "unary";
	}
	if (typ == COND) {
		return "cond";
	}
	if (typ == IDENT) {
		return "ident";
	}
	if (typ == CALL) {
		return "call";
	}
	if (typ == IF) {
		return "if";
	}
	if (typ == MEMBER_ACCESS) {
		return "member_access";
	}
	if (typ == PAREN) {
		return "paren";
	}
	if (typ == INDEX) {
		return "index";
	}
	if (typ == MATCH) {
		return "match";
	}
	if (typ == RANGE) {
		return "range";
	}
	if (typ == TMP_VAR) {
		return "tmp_var";
	}
	if (typ == BLOCK_EXPR) {
		return "block_expr";
	}
	if (typ == IMPORT) {
		return "import";
	}
	if (typ == LITERAL) {
		return "literal";
	}
	if (typ == INT_LITERAL) {
		return "int_literal";
	}
	if (typ == BOOL_LITERAL) {
		return "bool_literal";
	}
	if (typ == STR_LITERAL) {
		return "str_literal";
	}
	if (typ == INPUT) {
		return "input";
	}
	if (typ == OUTPUT) {
		return "output";
	}
	if (typ == CONFIG) {
		return "config";
	}
	if (typ == STMT) {
		return "stmt";
	}
	if (typ == LOOP) {
		return "loop";
	}
	if (typ == INDENT_BLOCK) {
		return "indent_block";
	}
	if (typ == MATCH_BRANCH) {
		return "match_branch";
	}
	if (typ == RETURN) {
		return "return";
	}
	if (typ == BREAK) {
		return "break";
	}
	if (typ == CONTINUE) {
		return "continue";
	}
	if (typ == ASSERT) {
		return "assert";
	}
	if (typ == IMPLICIT_YIELD) {
		return "implicit_yield";
	}
	if (typ == MEMBER) {
		return "member";
	}
	if (typ == FIELD) {
		return "field";
	}
	if (typ == FORMAT) {
		return "format";
	}
	if (typ == FUNCTION) {
		return "function";
	}
	if (typ == TYPE) {
		return "type";
	}
	if (typ == INT_TYPE) {
		return "int_type";
	}
	if (typ == IDENT_TYPE) {
		return "ident_type";
	}
	if (typ == INT_LITERAL_TYPE) {
		return "int_literal_type";
	}
	if (typ == STR_LITERAL_TYPE) {
		return "str_literal_type";
	}
	if (typ == VOID_TYPE) {
		return "void_type";
	}
	if (typ == BOOL_TYPE) {
		return "bool_type";
	}
	if (typ == ARRAY_TYPE) {
		return "array_type";
	}
	if (typ == FUNCTION_TYPE) {
		return "function_type";
	}
	if (typ == STRUCT_TYPE) {
		return "struct_type";
	}
	if (typ == UNION_TYPE) {
		return "union_type";
	}
	if (typ == CAST) {
		return "cast";
	}
	if (typ == COMMENT) {
		return "comment";
	}
	if (typ == COMMENT_GROUP) {
		return "comment_group";
	}
	if (typ == UNION_FIELD) {
		return "union_field";
	}
	if (typ == UNION_CANDIDATE) {
		return "union_candidate";
	}
	return NULL;
}

Ast* parse_json_to_ast(json_handlers* h,void* root_obj) {
	Ast* ast = (Ast*)h->alloc(h, sizeof(Ast), alignof(Ast));
	if (!ast) {
		return NULL;
	}
	ast->node = NULL;
	ast->node_size = 0;
	ast->scope = NULL;
	ast->scope_size = 0;
	void* node = h->object_get(h, root_obj, "node");
	if (!node) {
		goto error;
	}
	if(!h->is_array(h,node)) {
		goto error;
	}
	size_t node_size = h->array_size(h, node);
	ast->node = (RawNode*)h->alloc(h, sizeof(RawNode) * node_size, alignof(RawNode));
	if (!ast->node) {
		goto error;
	}
	ast->node_size = node_size;
	size_t i = 0;
	for (i=0; i < node_size; i++) {
		void* n = h->array_get(h, node, i);
		if (!n) {
			goto error;
		}
		void* loc = h->object_get(h, n, "loc");
		if (!loc) {
			goto error;
		}
		void* loc_line = h->object_get(h, loc, "line");
		if (!loc_line) {
			goto error;
		}
		void* loc_col = h->object_get(h, loc, "col");
		if (!loc_col) {
			goto error;
		}
		void* loc_file = h->object_get(h, loc, "file");
		if (!loc_file) {
			goto error;
		}
		void* loc_pos = h->object_get(h, loc, "pos");
		if (!loc_pos) {
			goto error;
		}
		void* loc_pos_begin = h->object_get(h, loc_pos, "begin");
		if (!loc_pos_begin) {
			goto error;
		}
		void* loc_pos_end = h->object_get(h, loc_pos, "end");
		if (!loc_pos_end) {
			goto error;
		}
		void* body = h->object_get(h, n, "body");
		if (!body) {
			goto error;
		}
		if(!h->is_object(h,body)) {
			goto error;
		}
		void* node_type = h->object_get(h, n, "node_type");
		if (!node_type) {
			goto error;
		}
		const char* node_type_str = h->string_get(h, node_type);
		if (!node_type_str) {
			goto error;
		}
		NodeType typ;
		if (!NodeType_from_string(node_type_str, &typ)) {
			goto error;
		}
		*(NodeType*)&ast->node[i].node_type = typ;
		if (!h->number_get(h, loc_line, &ast->node[i].loc.line)) {
			goto error;
		}
		if (!h->number_get(h, loc_col, &ast->node[i].loc.col)) {
			goto error;
		}
		if(!h->number_get(h,loc_file,&ast->node[i].loc.file)) {
			goto error;
		}
		if (!h->number_get(h, loc_pos_begin, &ast->node[i].loc.pos.begin)) {
			goto error;
		}
		if (!h->number_get(h, loc_pos_end, &ast->node[i].loc.pos.end)) {
			goto error;
		}
		ast->node[i].body = body;
	}
	void* scope = h->object_get(h, root_obj, "scope");
	if (!scope) {
		goto error;
	}
	if(!h->is_array(h,scope)) {
		goto error;
	}
	size_t scope_size = h->array_size(h, scope);
	ast->scope = (RawScope*)h->alloc(h, sizeof(RawScope) * scope_size, alignof(RawScope));
	if (!ast->scope) {
		goto error;
	}
	ast->scope_size = scope_size;
	size_t scope_ident_step = 0;
	for (i=0; i < scope_size; i++) {
		void* s = h->array_get(h, scope, i);
		if (!s) {
			goto error;
		}
		void* prev = h->object_get(h, s, "prev");
		if (!prev) {
			goto error;
		}
		void* next = h->object_get(h, s, "next");
		if (!next) {
			goto error;
		}
		void* branch = h->object_get(h, s, "branch");
		if (!branch) {
			goto error;
		}
		void* is_global = h->object_get(h, s, "is_global");
		if (!is_global) {
			goto error;
		}
		void* ident = h->object_get(h, s, "ident");
		if (!ident) {
			goto error;
		}
		if(!h->is_array(h,ident)) {
			goto error;
		}
		size_t ident_size = h->array_size(h, ident);
		ast->scope[i].ident = (uint64_t*)h->alloc(h, sizeof(uint64_t) * ident_size, alignof(uint64_t));
		if (!ast->scope[i].ident) {
			goto error;
		}
		scope_ident_step++;
		ast->scope[i].ident_size = ident_size;
		if (!h->number_get(h, prev, &ast->scope[i].prev)) {
			goto error;
		}
		if (!h->number_get(h, next, &ast->scope[i].next)) {
			goto error;
		}
		if (!h->number_get(h, branch, &ast->scope[i].branch)) {
			goto error;
		}
		if (!h->boolean_get(h, is_global)) {
			goto error;
		}
		size_t j = 0;
		for (j=0; j < ident_size; j++) {
			void* id = h->array_get(h, ident, j);
			if (!id) {
				goto error;
			}
			if (!h->number_get(h, id, &ast->scope[i].ident[j])) {
				goto error;
			}
		}
	}
	return ast;
error:
	h->free(h, ast->node);
for (i=0; i < scope_ident_step; i++) {
	h->free(h, ast->scope[i].ident);
}
	h->free(h, ast->scope);
	return NULL;
}

#ifdef __cplusplus
}
#endif

