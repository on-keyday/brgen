---
title: "BNF"
weight: 1
# bookFlatSection: false
# bookToc: true
# bookHidden: false
# bookCollapseSection: false
# bookComments: false
# bookSearchExclude: false
---

# BNF of brgen language

構文解析ルール

- 今のところドキュメンテーション用です。完全には [src/core/ast/parse.h](https://github.com/on-keyday/brgen/blob/main/src/core/ast/parse.h) にある実際の構文解析コード通りでない部分もあります。
- 一部の構文は意味解析による AST のノードの変更によって実現されているため、BNF から文法を完全に理解できるわけではありません。これとは別に意味論も理解する必要があります。

```
<space> := " " | "\t"
<line> := "\r\n" | "\r" | "\n"
<comment> := "#" <any unicode char> <line>
<indent> := <spaces at beginning of line followed by character except '#'>
<skip lines> := *(<space> | <comment> | <line>)
<skip white> := *(<indent> | <space> | <comment> | <line>)
<skip space> := *(<space> | <comment>)

<program> := <skip line> *(<statement> <skip>) <eof>
<statement> := <loop> | <format> | <state> | <enum> | <fn> | <return> | <break> | <continue> | <field> | <expr>
<indent block> := ":" <skip space> <line> +(<indent> <statement> <skip line>)
<loop> := "for" (<range loop> | <expr>? (";" <expr>? (";" <expr>?)?)?)  <indent block>
<range loop> := <ident> "in" <expr>
<format> := "format" <ident> <indent block>
<state> := "state" <ident> <indent block>
<enum> := "enum" <ident> <skip space> ":" <skip space> <line> (<indent> <anonymous field>)? +(<indent> <enum member> <skip line>)
<enum member> := <ident> ("=" (<expr> | <expr> "," <string literal> | <string literal> "," <expr> | <string literal>))?
<fn> := "fn" <ident> <fn parameter> ("->" <type>) <indent block>
<fn parameter> := "(" (<field> *("," <field>))? ")"
<return> := "return" <skip space> (<line> | <expr>)
<break> := "break"
<continue> := "continue"

<expr> := <comma>
<comma> := <assign> <skip space> *( "," <skip white> <assign> <skip space>)
<assign> := (<ident> | <special literal>) *(<member access> | <index>) <skip space> ("::=" | ":=" | "=" ) <skip white> <range> | <range>
<range> := <cond>? (".." | "..=") <cond>?
<cond> := <logical or> ("?" <skip white> <logical or> ":" <skip white> <cond>)
<logical or> := <logical and> <skip space> *("||" <skip white> <logical and> <skip space>)
<logical and> := <compare> <skip space> *("&&" <skip white> <compare> <skip space>)
<compare> := <add> <skip space> *(("==" | "!=" | "<" | "<=" | ">" | ">=") <skip white> <add> <skip space>)
<add> := <mul> <skip space> *(("+" | "-" | "|" | "^" ) <skip white> <mul> <skip space>)
<mul> := <unary> <skip space> *(("*" | "/" | "%" | "&" | "<<<" | ">>>" | "<<" | ">>" ) <skip white> <unary> <skip space>)
<unary> := ("-" | "!") <unary> | <postfix>
<postfix> := <prim> *(<member access> | <index> | <call>)
<member access> := "." <ident>
<index> := "[" <expr> "]"
<call> := "(" (<expr> *("," <expr>))? ")"
<prim> := <int literal> | <bool literal> | <string literal> | <regex literal> | <char literal> | <special literal> | <type literal> | <paran> | <if> | <match> | <ident>

<hex digit> := [0-9]|[a-f]|[A-F]
<binary digit> := "0" | "1"
<oct digit> := [0-7]
<digit> :=[0-9]
<int literal> := +<digit> | "0x" +<hex digit> | "0b" +<binary digit>
<bool literal> := "true" | "false"
<string literal> := "\""  *(<escape sequence> | <any unicode char except '"'>) "\""
<regex literal> := "/" *(<escape sequence> | <any unicode char except '/'>) "/"
<char literal> := "'" (<escape sequence> | <any unicode char except "'">) "'"
<special literal> := "input" | "output" | ""
<type literal> := "<" <type> ">"
<paran> := "(" <expr> ")"
<if> := "if" <expr> <indent block> *("elif" <expr> <indent block>) ("else" <indent block>)?
<match> := "match" <expr>? <skip white> ":" <skip space> <line> +<match branch>
<match branch> := <expr> ("=>" <statement> | <indent block>)

<ident> := <any unicode characters except control character or characters used for other usage (symbol or keyword)>

<escape sequence> := "\" ( "\" | "/" | "\"" | "'" |"x" <hex digit> <hex digit> | "n" | "r" | "t" | "u" <hex digit> <hex digit> <hex digit> <hex digit> )

<field> := <ident>? <anonymous field>
<anonymous field> := ":" <type> <call>?

<type> := <fn type> | "[" <expr>? "]" <type> | <str literal> | <ident> ("." <ident>)?
<fn type> := "fn" <skip white> "(" (<fn type param> *("," <fn type param>))? ")" ("->" <type>)?
<fn type param> := <ident>? ":" <type>
```
