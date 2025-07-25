#include "tree_sitter/parser.h"
#include <wchar.h>
#include "tree_sitter/alloc.h"
#include <string.h>
#include <stdio.h>

// Define a maximum stack depth for indentation levels
#define MAX_INDENT_LEVELS 255

// This enum must match the order of externals in grammar.js
enum TokenType {
    NEW_INDENT,     // or _indent if using symbols
    SAME_INDENT,    // or _same_indent
    DEDENT,         // or _dedent
    SKIPPED_SPACE,  // or _space
};

// State for the scanner
typedef struct {
    // We'll use a simple array as a stack for indentation levels
    unsigned short indent_stack[MAX_INDENT_LEVELS];
    unsigned short stack_size;
} Scanner;

bool do_indent(Scanner *scanner, unsigned short indent_level) {
    if (scanner->stack_size >= MAX_INDENT_LEVELS) {
        // Handle error: stack overflow
        return false;
    }
    scanner->indent_stack[scanner->stack_size++] = indent_level;  // Push a new indent level
    return true;
}

// --- Helper Functions (inside your scanner.c) ---

// Advance the lexer by one character
static void advance(TSLexer *lexer) {
    lexer->advance(lexer, false);
}

// Skip whitespace and comments
static void skip(TSLexer *lexer) {
    while (iswspace(lexer->lookahead) || lexer->lookahead == '#') {
        if (lexer->lookahead == '#') {  // Skip single-line comments
            while (lexer->lookahead != '\n' && lexer->lookahead != WEOF) {
                advance(lexer);
            }
            if (lexer->lookahead == '\n') {
                advance(lexer);  // comments line is not a newline
            }
        }
        else {
            advance(lexer);
        }
    }
}

// Read the indentation level of the current line
static unsigned short measure_indent(TSLexer *lexer) {
    unsigned short indent = 0;
    while (lexer->lookahead == ' ') {
        indent++;
        advance(lexer);
    }
    // You might need to handle tabs if your language uses them
    // If tabs are allowed, you'll need a tab_stop size (e.g., 4 or 8 spaces per tab)
    // while (lexer->lookahead == '\t') {
    //     indent += 8; // Example: 8 spaces per tab
    //     advance(lexer);
    // }
    return indent;
}

// --- Required Tree-sitter Scanner Functions ---

// Called to create the scanner's state
void *tree_sitter_brgen_external_scanner_create() {
    Scanner *scanner = (Scanner *)ts_malloc(sizeof(Scanner));
    memset(scanner, 0, sizeof(Scanner));  // Initialize the scanner state
    scanner->stack_size = 1;
    scanner->indent_stack[0] = 0;  // Base indentation level
    // printf("Scanner created with base indentation level: %u %u %p\n", scanner->stack_size, scanner->indent_stack[0], scanner);
    return scanner;
}

// Called to destroy the scanner's state
void tree_sitter_brgen_external_scanner_destroy(void *payload) {
    // printf("Destroying scanner: %p\n", payload);
    Scanner *scanner = (Scanner *)payload;
    ts_free(scanner);
}

// Called to serialize the scanner's state (for incremental parsing)
unsigned tree_sitter_brgen_external_scanner_serialize(void *payload, char *buffer) {
    Scanner *scanner = (Scanner *)payload;
    if (scanner->stack_size >= MAX_INDENT_LEVELS) {
        // Handle error: stack overflow
        return 0;
    }
    buffer[0] = (char)scanner->stack_size;
    memcpy(&buffer[1], scanner->indent_stack, scanner->stack_size * sizeof(unsigned short));
    return 1 + scanner->stack_size * sizeof(unsigned short);
}

// Called to deserialize the scanner's state (for incremental parsing)
void tree_sitter_brgen_external_scanner_deserialize(void *payload, const char *buffer, unsigned length) {
    Scanner *scanner = (Scanner *)payload;
    if (length > 0) {
        scanner->stack_size = (unsigned short)buffer[0];
        memcpy(scanner->indent_stack, &buffer[1], scanner->stack_size * sizeof(unsigned short));
    }
    else {
        scanner->stack_size = 1;       // Reset to base state if no data
        scanner->indent_stack[0] = 0;  // Reset to base indentation level
    }
}

// The core scanning logic
bool tree_sitter_brgen_external_scanner_scan(void *payload, TSLexer *lexer, const bool *valid_symbols) {
    Scanner *scanner = (Scanner *)payload;
#define printf(...) \
    lexer->log(lexer, __VA_ARGS__);
    printf("Valid symbols: %s%s%s%s at %d\n",
           valid_symbols[NEW_INDENT] ? "NEW_INDENT " : "",
           valid_symbols[SAME_INDENT] ? "SAME_INDENT " : "",
           valid_symbols[DEDENT] ? "DEDENT " : "",
           valid_symbols[SKIPPED_SPACE] ? "SKIPPED_SPACE" : "",
           lexer->get_column(lexer));
    unsigned short current_indent;
    int index = scanner->stack_size - 1;
    printf("  Current stack size: %u, Index: %d\n", scanner->stack_size, index);
    unsigned short last_indent = scanner->indent_stack[index];
    printf("  Last indent: %u\n", last_indent);
    if (lexer->lookahead == '\r') {
        advance(lexer);  // Handle CRLF
    }
    if (lexer->lookahead == '\n') {
        advance(lexer);  // Skip newline
        current_indent = measure_indent(lexer);

        printf("  Current indent: %u\n", current_indent);
        if (valid_symbols[NEW_INDENT]) {
            if (current_indent > last_indent) {
                printf("  Indentation increased: %u (last: %u)\n", current_indent, last_indent);
                lexer->result_symbol = NEW_INDENT;
                lexer->mark_end(lexer);
                return do_indent(scanner, current_indent);
            }
        }
        if (valid_symbols[SAME_INDENT] && scanner->stack_size > 1) {
            if (current_indent == last_indent) {
                lexer->result_symbol = SAME_INDENT;
                lexer->mark_end(lexer);
                return true;  // Match same indent
            }
        }
        if (valid_symbols[DEDENT] && scanner->stack_size > 1) {
            if (current_indent < last_indent) {
                printf("  Indentation decreased: %u (last: %u)\n", current_indent, last_indent);
                lexer->result_symbol = DEDENT;
                lexer->mark_end(lexer);  // currently mark as dedent at here
                scanner->stack_size--;   // first pop, but not clear
                while (true) {
                    // try read next indent level by skipping spaces
                    if (lexer->lookahead == WEOF) {
                        return true;  // Match dedent at EOF
                    }
                    if (lexer->lookahead == '\r') {
                        advance(lexer);  // Handle CRLF
                    }
                    // if line exists, then it may be same indent later
                    if (lexer->lookahead == '\n') {
                        advance(lexer);
                        int next_indent = measure_indent(lexer);
                        if (lexer->lookahead == WEOF || lexer->lookahead == '\n' ||
                            lexer->lookahead == '\r' || lexer->lookahead == '#') {
                            continue;
                        }
                        if (next_indent == last_indent) {
                            // special case: same indent after pseudo-dedent
                            printf("  Same indent after dedent: %u\n", last_indent);
                            lexer->result_symbol = SAME_INDENT;
                            lexer->mark_end(lexer);
                            scanner->stack_size++;  // Push back the indent level
                            return true;            // Match same indent after dedent
                        }
                        continue;
                    }
                    if (lexer->lookahead == '#') {
                        advance(lexer);  // Skip comment start
                        // Skip comments
                        while (lexer->lookahead != '\n' && lexer->lookahead != WEOF) {
                            advance(lexer);
                        }
                        continue;  // Re-check the next character
                    }
                    return true;  // Match dedent
                }
            }
        }
        if (valid_symbols[SKIPPED_SPACE]) {
            // If we hit EOF or a comment, we can skip the space
            lexer->result_symbol = SKIPPED_SPACE;
            lexer->mark_end(lexer);
            printf("  Skipped space at EOF or comment\n");
            return true;  // Match skipped space
        }
    }
    else if (valid_symbols[SKIPPED_SPACE] && (lexer->lookahead == ' ' || lexer->lookahead == '\t')) {
        // If we encounter whitespace but not a newline, we can skip it
        advance(lexer);  // Skip the whitespace character
        lexer->result_symbol = SKIPPED_SPACE;
        lexer->mark_end(lexer);
        printf("  Skipped space\n");
        return true;  // Match skipped space
    }
    else if (lexer->lookahead == WEOF) {
        // Handle end of file
        if (valid_symbols[DEDENT] && scanner->stack_size > 1) {
            printf("  EOF reached, dedenting\n");
            lexer->result_symbol = DEDENT;
            lexer->mark_end(lexer);
            scanner->stack_size--;  // Pop the last indent level
            return true;            // Match dedent at EOF
        }
    }
    return false;  // No match
}