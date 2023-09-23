package ast2go

import (
	"encoding/json"
	"errors"
	"fmt"
	"reflect"
)

type Node interface {
	node()
}

type Pos struct {
	Begin int `json:"begin"`
	End   int `json:"end"`
}

type Loc struct {
	Pos  Pos `json:"pos"`
	File int `json:"file"`
}

type nodeHdr struct {
	NodeType string `json:"node_type"`
	Loc      Loc    `json:"loc"`
}

type rawNode struct {
	nodeHdr
	Body json.RawMessage `json:"body"`
}

type Object interface {
	Identifier() string
}

type Scope struct {
	Prev    *Scope
	Next    *Scope
	Objects []Object
	Branch  *Scope
}

type astConstructor struct {
	nodes  []Node
	scopes []*Scope
}

type AST struct {
	*Program
}

type jsonScope struct {
	Prev    *uint64 `json:"prev"`
	Next    *uint64 `json:"next"`
	Objects []uint64
	Branch  *uint64 `json:"branch"`
}

type jsonAST struct {
	RawNodes  []rawNode   `json:"node"`
	RawScopes []jsonScope `json:"scope"`
}

type ExprNode interface {
	Node
	exprNode()
	Type() TypeNode
}

type LiteralNode interface {
	ExprNode
	literalNode()
}

type TypeNode interface {
	Node
	typeNode()
}

type StmtNode interface {
	Node
	stmtNode()
}

type MemberNode interface {
	StmtNode
	memberNode()
}

type IndentScopeNode struct {
	Loc      Loc
	Elements []Node
	Scope    *Scope
}

type IdentUsage int

const (
	IdentUsageUnknown IdentUsage = iota
	IdentUsageReference
	IdentUsageDefineVariable
	IdentUsageDefineConst
	IdentUsageDefineField
	IdentUsageDefineFormat
	IdentUsageDefineFn
	IdentUsageReferenceType
)

func (i IdentUsage) String() string {
	switch i {
	case IdentUsageUnknown:
		return "unknown"
	case IdentUsageReference:
		return "reference"
	case IdentUsageDefineVariable:
		return "define_variable"
	case IdentUsageDefineConst:
		return "define_const"
	case IdentUsageDefineField:
		return "define_field"
	case IdentUsageDefineFormat:
		return "define_format"
	case IdentUsageDefineFn:
		return "define_fn"
	case IdentUsageReferenceType:
		return "reference_type"
	default:
		return "unknown"
	}
}

type IdentNode struct {
	Loc      Loc
	ExprType TypeNode
	Name     string
	Base     *IdentNode
	Scope    *Scope
	Usage    IdentUsage
}

type CallNode struct {
	Loc      Loc
	ExprType TypeNode
	Callee   ExprNode
	Args     []ExprNode
	EndLoc   Loc
}

type ParenNode struct {
	Loc      Loc
	ExprType TypeNode
	Expr     ExprNode
	EndLoc   Loc
}

type IfNode struct {
	Loc      Loc
	ExprType TypeNode
	Cond     ExprNode
	Then     *IndentScopeNode
	Else     Node
	EndLoc   Loc
}

type UnaryNode struct {
	Loc      Loc
	ExprType TypeNode
	Op       string
	Expr     ExprNode
}

type BinaryNode struct {
	Loc      Loc
	ExprType TypeNode
	Op       string
	Left     ExprNode
	Right    ExprNode
}

type RangeNode struct {
	Loc      Loc
	ExprType TypeNode
	Op       string
	Begin    ExprNode
	End      ExprNode
}

type MemberAccessNode struct {
	Loc      Loc
	ExprType TypeNode
	Target   ExprNode
	Member   string
}

type CondNode struct {
	Loc      Loc
	ExprType TypeNode
	Cond     ExprNode
	Then     ExprNode
	Else     ExprNode
}

type IndexNode struct {
	Loc      Loc
	ExprType TypeNode
	Expr     ExprNode
	Index    ExprNode
	EndLoc   Loc
}

type MatchBranchNode struct {
	Loc  Loc
	Cond ExprNode
	Then Node
}

type MatchNode struct {
	Loc      Loc
	ExprType TypeNode
	Cond     ExprNode
	Branch   []*MatchBranchNode
}

type IntLiteralNode struct {
	Loc      Loc
	ExprType TypeNode
	Value    string
}

type StringLiteralNode struct {
	Loc      Loc
	ExprType TypeNode
	Value    string
}

type BoolLiteralNode struct {
	Loc      Loc
	ExprType TypeNode
	Value    bool
}

type InputNode struct {
	Loc      Loc
	ExprType TypeNode
}

type OutputNode struct {
	Loc      Loc
	ExprType TypeNode
}

type ConfigNode struct {
	Loc      Loc
	ExprType TypeNode
}

type FormatNode struct {
	Loc    Loc
	IsEnum bool
	Ident  *IdentNode
	Body   *IndentScopeNode
	Scope  *Scope
	Belong *FormatNode
}

type FieldNode struct {
	Loc    Loc
	Ident  *IdentNode
	Type   TypeNode
	Args   []ExprNode
	Scope  *Scope
	Belong *FormatNode
}

type LoopNode struct {
	Loc  Loc
	Init ExprNode
	Cond ExprNode
	Step ExprNode
	Body *IndentScopeNode
}

type FunctionNode struct {
	Loc    Loc
	Ident  *IdentNode
	Args   []*FieldNode
	Return TypeNode
	Body   *IndentScopeNode
	Scope  *Scope
}

type IntType struct {
	Loc     Loc
	BitSize int
	Raw     string
}

type IntLiteralType struct {
	Loc  Loc
	Base *IntLiteralNode
}

type StringLiteralType struct {
	Loc  Loc
	Base *StringLiteralNode
}

type BoolType struct {
	Loc Loc
}

type IdentType struct {
	Loc   Loc
	Ident *IdentNode
	Base  *FormatNode
}

type VoidType struct {
	Loc Loc
}

type ArrayType struct {
	Loc    Loc
	Base   TypeNode
	Length ExprNode
}

type StructType struct {
	Loc    Loc
	Fields []MemberNode
}

type UnionType struct {
	Loc    Loc
	Fields []*StructType
}

type FunctionTypeNode struct {
	Loc    Loc
	Args   []TypeNode
	Return TypeNode
}

type Program struct {
	Loc         Loc
	Elements    []Node
	GlobalScope *Scope
}

type ImportNode struct {
	Loc        Loc
	ExprType   TypeNode
	Path       string
	Base       *CallNode
	ImportDesc *Program
}

func (i *IdentNode) node()     {}
func (i *IdentNode) exprNode() {}
func (i *IdentNode) Identifier() string {
	return i.Name
}
func (i *IdentNode) Type() TypeNode {
	return i.ExprType
}

func (i *CallNode) node()     {}
func (i *CallNode) exprNode() {}
func (i *CallNode) Type() TypeNode {
	return i.ExprType
}

func (i *ParenNode) node()     {}
func (i *ParenNode) exprNode() {}
func (i *ParenNode) Type() TypeNode {
	return i.ExprType
}

func (i *IfNode) node()     {}
func (i *IfNode) exprNode() {}
func (i *IfNode) Type() TypeNode {
	return i.ExprType
}

func (i *UnaryNode) node()     {}
func (i *UnaryNode) exprNode() {}
func (i *UnaryNode) Type() TypeNode {
	return i.ExprType
}

func (i *BinaryNode) node()     {}
func (i *BinaryNode) exprNode() {}
func (i *BinaryNode) Type() TypeNode {
	return i.ExprType
}

func (i *RangeNode) node()     {}
func (i *RangeNode) exprNode() {}
func (i *RangeNode) Type() TypeNode {
	return i.ExprType
}

func (i *MemberAccessNode) node()     {}
func (i *MemberAccessNode) exprNode() {}
func (i *MemberAccessNode) Type() TypeNode {
	return i.ExprType
}

func (i *CondNode) node()     {}
func (i *CondNode) exprNode() {}
func (i *CondNode) Type() TypeNode {
	return i.ExprType
}

func (i *IndexNode) node()     {}
func (i *IndexNode) exprNode() {}
func (i *IndexNode) Type() TypeNode {
	return i.ExprType
}

func (i *MatchBranchNode) node()     {}
func (i *MatchBranchNode) stmtNode() {}

func (i *MatchNode) node()     {}
func (i *MatchNode) exprNode() {}
func (i *MatchNode) Type() TypeNode {
	return i.ExprType
}

func (i *IntLiteralNode) node()        {}
func (i *IntLiteralNode) exprNode()    {}
func (i *IntLiteralNode) literalNode() {}
func (i *IntLiteralNode) Type() TypeNode {
	return i.ExprType
}

func (i *StringLiteralNode) node()        {}
func (i *StringLiteralNode) exprNode()    {}
func (i *StringLiteralNode) literalNode() {}
func (i *StringLiteralNode) Type() TypeNode {
	return i.ExprType
}

func (i *BoolLiteralNode) node()        {}
func (i *BoolLiteralNode) exprNode()    {}
func (i *BoolLiteralNode) literalNode() {}
func (i *BoolLiteralNode) Type() TypeNode {
	return i.ExprType
}

func (i *InputNode) node()        {}
func (i *InputNode) exprNode()    {}
func (i *InputNode) literalNode() {}
func (i *InputNode) Type() TypeNode {
	return i.ExprType
}

func (i *OutputNode) node()        {}
func (i *OutputNode) exprNode()    {}
func (i *OutputNode) literalNode() {}
func (i *OutputNode) Type() TypeNode {
	return i.ExprType
}

func (i *ConfigNode) node()        {}
func (i *ConfigNode) exprNode()    {}
func (i *ConfigNode) literalNode() {}
func (i *ConfigNode) Type() TypeNode {
	return i.ExprType
}

func (i *FormatNode) node()     {}
func (i *FormatNode) stmtNode() {}
func (i *FormatNode) Identifier() string {
	return i.Ident.Name
}

func (i *FieldNode) node()     {}
func (i *FieldNode) stmtNode() {}
func (i *FieldNode) Identifier() string {
	return i.Ident.Name
}
func (i *FieldNode) memberNode() {}

func (i *LoopNode) node()     {}
func (i *LoopNode) stmtNode() {}

func (i *FunctionNode) node()     {}
func (i *FunctionNode) stmtNode() {}
func (i *FunctionNode) Identifier() string {
	return i.Ident.Name
}
func (i *FunctionNode) memberNode() {}

func (i *IntType) node()     {}
func (i *IntType) typeNode() {}

func (i *IntLiteralType) node()     {}
func (i *IntLiteralType) typeNode() {}

func (i *StringLiteralType) node()     {}
func (i *StringLiteralType) typeNode() {}

func (i *BoolType) node()     {}
func (i *BoolType) typeNode() {}

func (i *IdentType) node()     {}
func (i *IdentType) typeNode() {}

func (i *VoidType) node()     {}
func (i *VoidType) typeNode() {}

func (i *ArrayType) node()     {}
func (i *ArrayType) typeNode() {}

func (i *StructType) node()     {}
func (i *StructType) typeNode() {}

func (i *UnionType) node()     {}
func (i *UnionType) typeNode() {}

func (i *FunctionTypeNode) node()     {}
func (i *FunctionTypeNode) typeNode() {}

func (i *IndentScopeNode) node()     {}
func (i *IndentScopeNode) stmtNode() {}

func (i *Program) node() {}

func (i *ImportNode) node()     {}
func (i *ImportNode) exprNode() {}
func (i *ImportNode) Type() TypeNode {
	return i.ExprType
}

func (a *astConstructor) collectNodeLink(rawNodes []rawNode) error {
	for i, rawNode := range rawNodes {
		var node Node
		switch rawNode.NodeType {
		case "ident":
			node = &IdentNode{}
		case "call":
			node = &CallNode{}
		case "paren":
			node = &ParenNode{}
		case "if":
			node = &IfNode{}
		case "unary":
			node = &UnaryNode{}
		case "binary":
			node = &BinaryNode{}
		case "range":
			node = &RangeNode{}
		case "member_access":
			node = &MemberAccessNode{}
		case "cond":
			node = &CondNode{}
		case "index":
			node = &IndexNode{}
		case "match_branch":
			node = &MatchBranchNode{}
		case "match":
			node = &MatchNode{}
		case "int_literal":
			node = &IntLiteralNode{}
		case "string_literal":
			node = &StringLiteralNode{}
		case "bool_literal":
			node = &BoolLiteralNode{}
		case "input":
			node = &InputNode{}
		case "output":
			node = &OutputNode{}
		case "config":
			node = &ConfigNode{}
		case "format":
			node = &FormatNode{}
		case "field":
			node = &FieldNode{}
		case "loop":
			node = &LoopNode{}
		case "function":
			node = &FunctionNode{}
		case "int_type":
			node = &IntType{}
		case "int_literal_type":
			node = &IntLiteralType{}
		case "string_literal_type":
			node = &StringLiteralType{}
		case "bool_type":
			node = &BoolType{}
		case "ident_type":
			node = &IdentType{}
		case "void_type":
			node = &VoidType{}
		case "array_type":
			node = &ArrayType{}
		case "struct_type":
			node = &StructType{}
		case "union_type":
			node = &UnionType{}
		case "function_type":
			node = &FunctionTypeNode{}
		case "indent_scope":
			node = &IndentScopeNode{}
		case "program":
			node = &Program{}
		case "import":
			node = &ImportNode{}
		default:
			return errors.New("unknown node type: " + rawNode.NodeType)
		}
		a.nodes[i] = node
	}
	return nil
}

func (a *astConstructor) collectScopeLink(scope []jsonScope) {
	for i := range a.scopes {
		a.scopes[i] = &Scope{}
	}
}

func (a *astConstructor) linkNode(rawNodes []rawNode) error {
	for i, rawNode := range rawNodes {
		body := rawNode.Body
		switch v := a.nodes[i].(type) {
		case *IdentNode:
			var identTmp struct {
				Name     string  `json:"ident"`
				ExprType *uint64 `json:"expr_type"`
				Base     *uint64 `json:"base"`
				Scope    *uint64 `json:"scope"`
				Usage    string  `json:"usage"`
			}
			if err := json.Unmarshal(body, &identTmp); err != nil {
				return err
			}
			v.Name = identTmp.Name
			if identTmp.ExprType != nil {
				v.ExprType = a.nodes[*identTmp.ExprType].(TypeNode)
			}
			if identTmp.Base != nil {
				v.Base = a.nodes[*identTmp.Base].(*IdentNode)
			}
			if identTmp.Scope != nil {
				v.Scope = a.scopes[*identTmp.Scope]
			}
			switch identTmp.Usage {
			case "unknown":
				v.Usage = IdentUsageUnknown
			case "reference":
				v.Usage = IdentUsageReference
			case "define_variable":
				v.Usage = IdentUsageDefineVariable
			case "define_const":
				v.Usage = IdentUsageDefineConst
			case "define_field":
				v.Usage = IdentUsageDefineField
			case "define_format":
				v.Usage = IdentUsageDefineFormat
			case "define_fn":
				v.Usage = IdentUsageDefineFn
			case "reference_type":
				v.Usage = IdentUsageReferenceType
			default:
				return errors.New("unknown ident usage: " + identTmp.Usage)
			}
		case *CallNode:
			var callTmp struct {
				ExprType *uint64  `json:"expr_type"`
				Callee   uint64   `json:"callee"`
				Args     []uint64 `json:"arguments"`
			}
			if err := json.Unmarshal(body, &callTmp); err != nil {
				return err
			}
			if callTmp.ExprType != nil {
				v.ExprType = a.nodes[*callTmp.ExprType].(TypeNode)
			}
			v.Callee = a.nodes[callTmp.Callee].(ExprNode)
			if callTmp.Args != nil {
				v.Args = make([]ExprNode, len(callTmp.Args))
				for i, arg := range callTmp.Args {
					v.Args[i] = a.nodes[arg].(ExprNode)
				}
			}
		case *Program:
			var programTmp struct {
				Elements []uint64 `json:"elements"`
				Scope    uint64   `json:"global_scope"`
			}
			if err := json.Unmarshal(body, &programTmp); err != nil {
				return err
			}
			v.Elements = make([]Node, len(programTmp.Elements))
			for i, element := range programTmp.Elements {
				v.Elements[i] = a.nodes[element]
			}
			v.GlobalScope = a.scopes[programTmp.Scope]
		case *ParenNode:
			var parenTmp struct {
				ExprType *uint64 `json:"expr_type"`
				Expr     uint64  `json:"expr"`
			}
			if err := json.Unmarshal(body, &parenTmp); err != nil {
				return err
			}
			if parenTmp.ExprType != nil {
				v.ExprType = a.nodes[*parenTmp.ExprType].(TypeNode)
			}
			v.Expr = a.nodes[parenTmp.Expr].(ExprNode)
		case *IfNode:
			var ifTmp struct {
				ExprType *uint64 `json:"expr_type"`
				Cond     uint64  `json:"cond"`
				Then     uint64  `json:"then"`
				Else     *uint64 `json:"els"`
			}
			if err := json.Unmarshal(body, &ifTmp); err != nil {
				return err
			}
			if ifTmp.ExprType != nil {
				v.ExprType = a.nodes[*ifTmp.ExprType].(TypeNode)
			}
			v.Cond = a.nodes[ifTmp.Cond].(ExprNode)
			v.Then = a.nodes[ifTmp.Then].(*IndentScopeNode)
			if ifTmp.Else != nil {
				v.Else = a.nodes[*ifTmp.Else]
			}
		case *UnaryNode:
			var unaryTmp struct {
				ExprType *uint64 `json:"expr_type"`
				Op       string  `json:"op"`
				Expr     uint64  `json:"expr"`
			}
			if err := json.Unmarshal(body, &unaryTmp); err != nil {
				return err
			}
			if unaryTmp.ExprType != nil {
				v.ExprType = a.nodes[*unaryTmp.ExprType].(TypeNode)
			}
			v.Op = unaryTmp.Op
			v.Expr = a.nodes[unaryTmp.Expr].(ExprNode)
		case *BinaryNode:
			var binaryTmp struct {
				ExprType *uint64 `json:"expr_type"`
				Op       string  `json:"op"`
				Left     uint64  `json:"left"`
				Right    uint64  `json:"right"`
			}
			if err := json.Unmarshal(body, &binaryTmp); err != nil {
				return err
			}
			if binaryTmp.ExprType != nil {
				v.ExprType = a.nodes[*binaryTmp.ExprType].(TypeNode)
			}
			v.Op = binaryTmp.Op
			v.Left = a.nodes[binaryTmp.Left].(ExprNode)
			v.Right = a.nodes[binaryTmp.Right].(ExprNode)
		case *RangeNode:
			var rangeTmp struct {
				ExprType *uint64 `json:"expr_type"`
				Op       string  `json:"op"`
				Begin    *uint64 `json:"begin"`
				End      *uint64 `json:"end"`
			}
			if err := json.Unmarshal(body, &rangeTmp); err != nil {
				return err
			}
			if rangeTmp.ExprType != nil {
				v.ExprType = a.nodes[*rangeTmp.ExprType].(TypeNode)
			}
			v.Op = rangeTmp.Op
			if rangeTmp.Begin != nil {
				v.Begin = a.nodes[*rangeTmp.Begin].(ExprNode)
			}
			if rangeTmp.End != nil {
				v.End = a.nodes[*rangeTmp.End].(ExprNode)
			}
		case *MemberAccessNode:
			var memberAccessTmp struct {
				ExprType *uint64 `json:"expr_type"`
				Target   uint64  `json:"target"`
				Member   string  `json:"member"`
			}
			if err := json.Unmarshal(body, &memberAccessTmp); err != nil {
				return err
			}
			if memberAccessTmp.ExprType != nil {
				v.ExprType = a.nodes[*memberAccessTmp.ExprType].(TypeNode)
			}
			v.Target = a.nodes[memberAccessTmp.Target].(ExprNode)
			v.Member = memberAccessTmp.Member
		case *CondNode:
			var condTmp struct {
				ExprType *uint64 `json:"expr_type"`
				Cond     uint64  `json:"cond"`
				Then     uint64  `json:"then"`
				Else     uint64  `json:"els"`
			}
			if err := json.Unmarshal(body, &condTmp); err != nil {
				return err
			}
			if condTmp.ExprType != nil {
				v.ExprType = a.nodes[*condTmp.ExprType].(TypeNode)
			}
			v.Cond = a.nodes[condTmp.Cond].(ExprNode)
			v.Then = a.nodes[condTmp.Then].(ExprNode)
			v.Else = a.nodes[condTmp.Else].(ExprNode)
		case *IndexNode:
			var indexTmp struct {
				ExprType *uint64 `json:"expr_type"`
				Expr     uint64  `json:"expr"`
				Index    uint64  `json:"index"`
			}
			if err := json.Unmarshal(body, &indexTmp); err != nil {
				return err
			}
			if indexTmp.ExprType != nil {
				v.ExprType = a.nodes[*indexTmp.ExprType].(TypeNode)
			}
			v.Expr = a.nodes[indexTmp.Expr].(ExprNode)
			v.Index = a.nodes[indexTmp.Index].(ExprNode)
		case *MatchBranchNode:
			var matchBranchTmp struct {
				Cond uint64 `json:"cond"`
				Then uint64 `json:"then"`
			}
			if err := json.Unmarshal(body, &matchBranchTmp); err != nil {
				return err
			}
			v.Cond = a.nodes[matchBranchTmp.Cond].(ExprNode)
			v.Then = a.nodes[matchBranchTmp.Then]
		case *MatchNode:
			var matchTmp struct {
				ExprType *uint64  `json:"expr_type"`
				Cond     *uint64  `json:"cond"`
				Branch   []uint64 `json:"branch"`
			}
			if err := json.Unmarshal(body, &matchTmp); err != nil {
				return err
			}
			if matchTmp.ExprType != nil {
				v.ExprType = a.nodes[*matchTmp.ExprType].(TypeNode)
			}
			if matchTmp.Cond != nil {
				v.Cond = a.nodes[*matchTmp.Cond].(ExprNode)
			}
			v.Branch = make([]*MatchBranchNode, len(matchTmp.Branch))
			for i, branch := range matchTmp.Branch {
				v.Branch[i] = a.nodes[branch].(*MatchBranchNode)
			}
		case *IntLiteralNode:
			var intLiteralTmp struct {
				ExprType *uint64 `json:"expr_type"`
				Value    string  `json:"value"`
			}
			if err := json.Unmarshal(body, &intLiteralTmp); err != nil {
				return err
			}
			if intLiteralTmp.ExprType != nil {
				v.ExprType = a.nodes[*intLiteralTmp.ExprType].(TypeNode)
			}
			v.Value = intLiteralTmp.Value
		case *StringLiteralNode:
			var stringLiteralTmp struct {
				ExprType *uint64 `json:"expr_type"`
				Value    string  `json:"value"`
			}

			if err := json.Unmarshal(body, &stringLiteralTmp); err != nil {
				return err
			}
			if stringLiteralTmp.ExprType != nil {
				v.ExprType = a.nodes[*stringLiteralTmp.ExprType].(TypeNode)
			}
			v.Value = stringLiteralTmp.Value

		case *BoolLiteralNode:
			var boolLiteralTmp struct {
				ExprType *uint64 `json:"expr_type"`
				Value    bool    `json:"value"`
			}
			if err := json.Unmarshal(body, &boolLiteralTmp); err != nil {
				return err
			}
			if boolLiteralTmp.ExprType != nil {
				v.ExprType = a.nodes[*boolLiteralTmp.ExprType].(TypeNode)
			}
			v.Value = boolLiteralTmp.Value
		case *InputNode:
			var inputTmp struct {
				ExprType *uint64 `json:"expr_type"`
			}
			if err := json.Unmarshal(body, &inputTmp); err != nil {
				return err
			}
			if inputTmp.ExprType != nil {
				v.ExprType = a.nodes[*inputTmp.ExprType].(TypeNode)
			}
		case *OutputNode:
			var outputTmp struct {
				ExprType *uint64 `json:"expr_type"`
			}
			if err := json.Unmarshal(body, &outputTmp); err != nil {
				return err
			}
			if outputTmp.ExprType != nil {
				v.ExprType = a.nodes[*outputTmp.ExprType].(TypeNode)
			}
		case *ConfigNode:
			var configTmp struct {
				ExprType *uint64 `json:"expr_type"`
			}
			if err := json.Unmarshal(body, &configTmp); err != nil {
				return err
			}
			if configTmp.ExprType != nil {
				v.ExprType = a.nodes[*configTmp.ExprType].(TypeNode)
			}
		case *FormatNode:
			var formatTmp struct {
				IsEnum bool    `json:"is_enum"`
				Ident  uint64  `json:"ident"`
				Body   uint64  `json:"body"`
				Scope  uint64  `json:"scope"`
				Belong *uint64 `json:"belong"`
			}
			if err := json.Unmarshal(body, &formatTmp); err != nil {
				return err
			}
			v.IsEnum = formatTmp.IsEnum
			v.Ident = a.nodes[formatTmp.Ident].(*IdentNode)
			v.Body = a.nodes[formatTmp.Body].(*IndentScopeNode)
			v.Scope = a.scopes[formatTmp.Scope]
			if formatTmp.Belong != nil {
				v.Belong = a.nodes[*formatTmp.Belong].(*FormatNode)
			}
		case *FieldNode:
			var fieldTmp struct {
				Ident  *uint64  `json:"ident"`
				Type   uint64   `json:"field_type"`
				Args   []uint64 `json:"arguments"`
				Scope  uint64   `json:"scope"`
				Belong *uint64  `json:"belong"`
			}
			if err := json.Unmarshal(body, &fieldTmp); err != nil {
				return err
			}
			if fieldTmp.Ident != nil {
				v.Ident = a.nodes[*fieldTmp.Ident].(*IdentNode)
			}
			v.Type = a.nodes[fieldTmp.Type].(TypeNode)
			v.Args = make([]ExprNode, len(fieldTmp.Args))
			for i, arg := range fieldTmp.Args {
				v.Args[i] = a.nodes[arg].(ExprNode)
			}
			v.Scope = a.scopes[fieldTmp.Scope]
			if fieldTmp.Belong != nil {
				v.Belong = a.nodes[*fieldTmp.Belong].(*FormatNode)
			}
		case *LoopNode:
			var loopTmp struct {
				Init *uint64 `json:"init"`
				Cond *uint64 `json:"cond"`
				Step *uint64 `json:"step"`
				Body uint64  `json:"body"`
			}
			if err := json.Unmarshal(body, &loopTmp); err != nil {
				return err
			}
			if loopTmp.Init != nil {
				v.Init = a.nodes[*loopTmp.Init].(ExprNode)
			}
			if loopTmp.Cond != nil {
				v.Cond = a.nodes[*loopTmp.Cond].(ExprNode)
			}
			if loopTmp.Step != nil {
				v.Step = a.nodes[*loopTmp.Step].(ExprNode)
			}
			v.Body = a.nodes[loopTmp.Body].(*IndentScopeNode)
		case *FunctionNode:
			var functionTmp struct {
				Ident  uint64   `json:"ident"`
				Args   []uint64 `json:"parameters"`
				Return uint64   `json:"return_type"`
				Body   uint64   `json:"body"`
				Scope  uint64   `json:"scope"`
			}
			if err := json.Unmarshal(body, &functionTmp); err != nil {
				return err
			}
			v.Ident = a.nodes[functionTmp.Ident].(*IdentNode)
			v.Args = make([]*FieldNode, len(functionTmp.Args))
			for i, arg := range functionTmp.Args {
				v.Args[i] = a.nodes[arg].(*FieldNode)
			}
			v.Return = a.nodes[functionTmp.Return].(TypeNode)
			v.Body = a.nodes[functionTmp.Body].(*IndentScopeNode)
			v.Scope = a.scopes[functionTmp.Scope]
		case *IntType:
			var intTypeTmp struct {
				BitSize int    `json:"bit_size"`
				Raw     string `json:"raw"`
			}
			if err := json.Unmarshal(body, &intTypeTmp); err != nil {
				return err
			}
			v.BitSize = intTypeTmp.BitSize
			v.Raw = intTypeTmp.Raw
		case *IntLiteralType:
			var intLiteralTypeTmp struct {
				Base uint64 `json:"base"`
			}
			if err := json.Unmarshal(body, &intLiteralTypeTmp); err != nil {
				return err
			}
			v.Base = a.nodes[intLiteralTypeTmp.Base].(*IntLiteralNode)
		case *StringLiteralType:
			var stringLiteralTypeTmp struct {
				Base uint64 `json:"base"`
			}
			if err := json.Unmarshal(body, &stringLiteralTypeTmp); err != nil {
				return err
			}
			v.Base = a.nodes[stringLiteralTypeTmp.Base].(*StringLiteralNode)
		case *BoolType:
		case *IdentType:
			var identTypeTmp struct {
				Ident uint64  `json:"ident"`
				Base  *uint64 `json:"base"`
			}
			if err := json.Unmarshal(body, &identTypeTmp); err != nil {
				return err
			}
			v.Ident = a.nodes[identTypeTmp.Ident].(*IdentNode)
			if identTypeTmp.Base != nil {
				v.Base = a.nodes[*identTypeTmp.Base].(*FormatNode)
			}
		case *VoidType:
		case *ArrayType:
			var arrayTypeTmp struct {
				BaseType uint64 `json:"base_type"`
				Length   uint64 `json:"length"`
			}
			if err := json.Unmarshal(body, &arrayTypeTmp); err != nil {
				return err
			}
			v.Base = a.nodes[arrayTypeTmp.BaseType].(TypeNode)
			v.Length = a.nodes[arrayTypeTmp.Length].(ExprNode)
		case *StructType:
			var structTypeTmp struct {
				Fields []uint64 `json:"fields"`
			}
			if err := json.Unmarshal(body, &structTypeTmp); err != nil {
				return err
			}
			v.Fields = make([]MemberNode, len(structTypeTmp.Fields))
			for i, field := range structTypeTmp.Fields {
				v.Fields[i] = a.nodes[field].(MemberNode)
			}
		case *UnionType:
			var unionTypeTmp struct {
				Fields []uint64 `json:"fields"`
			}
			if err := json.Unmarshal(body, &unionTypeTmp); err != nil {
				return err
			}
			v.Fields = make([]*StructType, len(unionTypeTmp.Fields))
			for i, field := range unionTypeTmp.Fields {
				v.Fields[i] = a.nodes[field].(*StructType)
			}
		case *FunctionTypeNode:
			var functionTypeTmp struct {
				Args   []uint64 `json:"parameters"`
				Return uint64   `json:"return_type"`
			}
			if err := json.Unmarshal(body, &functionTypeTmp); err != nil {
				return err
			}
			v.Args = make([]TypeNode, len(functionTypeTmp.Args))
			for i, arg := range functionTypeTmp.Args {
				v.Args[i] = a.nodes[arg].(TypeNode)
			}
		case *IndentScopeNode:
			var indentScopeTmp struct {
				Elements []uint64 `json:"elements"`
				Scope    uint64   `json:"scope"`
			}
			if err := json.Unmarshal(body, &indentScopeTmp); err != nil {
				return err
			}
			v.Elements = make([]Node, len(indentScopeTmp.Elements))
			for i, element := range indentScopeTmp.Elements {
				v.Elements[i] = a.nodes[element]
			}
			v.Scope = a.scopes[indentScopeTmp.Scope]
		case *ImportNode:
			var importTmp struct {
				ExprType   *uint64 `json:"expr_type"`
				Path       string  `json:"path"`
				Base       uint64  `json:"base"`
				ImportDesc uint64  `json:"import_desc"`
			}
			if err := json.Unmarshal(body, &importTmp); err != nil {
				return err
			}
			if importTmp.ExprType != nil {
				v.ExprType = a.nodes[*importTmp.ExprType].(TypeNode)
			}
			v.Path = importTmp.Path
			v.Base = a.nodes[importTmp.Base].(*CallNode)
			v.ImportDesc = a.nodes[importTmp.ImportDesc].(*Program)
		default:
			return errors.New("unknown node type: " + rawNode.NodeType)
		}
	}
	return nil
}

func (a *astConstructor) linkScope(s []jsonScope) {
	for i, scope := range s {
		if scope.Prev != nil {
			a.scopes[i].Prev = a.scopes[*scope.Prev]
		}
		if scope.Next != nil {
			a.scopes[i].Next = a.scopes[*scope.Next]
		}
		if scope.Branch != nil {
			a.scopes[i].Branch = a.scopes[*scope.Branch]
		}
		a.scopes[i].Objects = make([]Object, len(scope.Objects))
		for j, object := range scope.Objects {
			a.scopes[i].Objects[j] = a.nodes[object].(Object)
		}
	}
}

func (a *astConstructor) unmarshal(s []byte) (p *Program, err error) {
	defer func() {
		if r := recover(); r != nil {
			err = fmt.Errorf("type error: %v", r)
		}
	}()
	jsonAST := jsonAST{}
	if err := json.Unmarshal(s, &jsonAST); err != nil {
		return nil, err
	}
	a.nodes = make([]Node, len(jsonAST.RawNodes))
	a.scopes = make([]*Scope, len(jsonAST.RawScopes))
	err = a.collectNodeLink(jsonAST.RawNodes)
	if err != nil {
		return nil, err
	}
	a.collectScopeLink(jsonAST.RawScopes)
	if err = a.linkNode(jsonAST.RawNodes); err != nil {
		return nil, err
	}
	a.linkScope(jsonAST.RawScopes)
	return a.nodes[0].(*Program), nil
}

func (a *AST) UnmarshalJSON(s []byte) (err error) {
	c := astConstructor{}
	a.Program, err = c.unmarshal(s)
	return err
}

type File struct {
	File  []string `json:"file"`
	Ast   AST      `json:"ast"`
	Error string   `json:"error"`
}

func Walk(node Node, f func(child Node) error) error {
	switch v := node.(type) {
	case *IdentNode:
		if v.Base != nil {
			if err := f(v.Base); err != nil {
				return err
			}
		}
	case *CallNode:
		if err := f(v.Callee); err != nil {
			return err
		}
		for _, arg := range v.Args {
			if err := f(arg); err != nil {
				return err
			}
		}
	case *ParenNode:
		if err := f(v.Expr); err != nil {
			return err
		}
	case *IfNode:
		if err := f(v.Cond); err != nil {
			return err
		}
		if err := f(v.Then); err != nil {
			return err
		}
		if v.Else != nil {
			if err := f(v.Else); err != nil {
				return err
			}
		}
	case *UnaryNode:
		if err := f(v.Expr); err != nil {
			return err
		}
	case *BinaryNode:
		if err := f(v.Left); err != nil {
			return err
		}
		if err := f(v.Right); err != nil {
			return err
		}
	case *RangeNode:
		if v.Begin != nil {
			if err := f(v.Begin); err != nil {
				return err
			}
		}
		if v.End != nil {
			if err := f(v.End); err != nil {
				return err
			}
		}
	case *MemberAccessNode:
		if err := f(v.Target); err != nil {
			return err
		}
	case *CondNode:
		if err := f(v.Cond); err != nil {
			return err
		}
		if err := f(v.Then); err != nil {
			return err
		}
		if err := f(v.Else); err != nil {
			return err
		}
	case *IndexNode:
		if err := f(v.Expr); err != nil {
			return err
		}
		if err := f(v.Index); err != nil {
			return err
		}

	case *MatchBranchNode:
		if err := f(v.Cond); err != nil {
			return err
		}
		if err := f(v.Then); err != nil {
			return err
		}
	case *MatchNode:
		if err := f(v.Cond); err != nil {
			return err
		}
		for _, branch := range v.Branch {
			if err := f(branch); err != nil {
				return err
			}
		}
	case *IntLiteralNode:
	case *StringLiteralNode:
	case *BoolLiteralNode:
	case *InputNode:
	case *OutputNode:
	case *ConfigNode:
	case *FormatNode:
		if err := f(v.Ident); err != nil {
			return err
		}
		if err := f(v.Body); err != nil {
			return err
		}
		if v.Belong != nil {
			if err := f(v.Belong); err != nil {
				return err
			}
		}
	case *FieldNode:
		if v.Ident != nil {
			if err := f(v.Ident); err != nil {
				return err
			}
		}
		if err := f(v.Type); err != nil {
			return err
		}
		for _, arg := range v.Args {
			if err := f(arg); err != nil {
				return err
			}
		}
		if v.Belong != nil {
			if err := f(v.Belong); err != nil {
				return err
			}
		}
	case *LoopNode:
		if err := f(v.Body); err != nil {
			return err
		}
	case *FunctionNode:
		if err := f(v.Ident); err != nil {
			return err
		}
		for _, arg := range v.Args {
			if err := f(arg); err != nil {
				return err
			}
		}
		if err := f(v.Return); err != nil {
			return err
		}
		if err := f(v.Body); err != nil {
			return err
		}
	case *IntType:
	case *IntLiteralType:
		if err := f(v.Base); err != nil {
			return err
		}
	case *StringLiteralType:
		if err := f(v.Base); err != nil {
			return err
		}
	case *BoolType:
	case *IdentType:
		if err := f(v.Ident); err != nil {
			return err
		}
		if v.Base != nil {
			if err := f(v.Base); err != nil {
				return err
			}
		}
	case *VoidType:
	case *ArrayType:
		if err := f(v.Base); err != nil {
			return err
		}
		if err := f(v.Length); err != nil {
			return err
		}
	case *IndentScopeNode:
		for _, element := range v.Elements {
			if err := f(element); err != nil {
				return err
			}
		}
	case *Program:
		for _, element := range v.Elements {
			if err := f(element); err != nil {
				return err
			}
		}
	default:
		return errors.New("unknown node type: " + reflect.TypeOf(node).String())

	}
	return nil
}
