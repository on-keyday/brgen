package tree_sitter_brgen_test

import (
	"testing"

	tree_sitter "github.com/tree-sitter/go-tree-sitter"
	tree_sitter_brgen "github.com/on-keyday/brgen/bindings/go"
)

func TestCanLoadGrammar(t *testing.T) {
	language := tree_sitter.NewLanguage(tree_sitter_brgen.Language())
	if language == nil {
		t.Errorf("Error loading brgen grammar")
	}
}
