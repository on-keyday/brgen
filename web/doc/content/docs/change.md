---
title: "Language Change"
weight: 1
# bookFlatSection: false
# bookToc: true
# bookHidden: false
# bookCollapseSection: false
# bookComments: false
# bookSearchExclude: false
---

# 言語仕様変更記録

言語仕様の変更記録 (24/02/07 ～)

# 24/01/23 (転記)

- resolve_primitive_cast を parser の機能に移動、それに伴い --not-resolve-cast オプションを 削除

## 24/02/07

- const_variable を immutable_variable に変更
  - 語の意味の明確化

## 24/3/??

- loop 文を for 文に
- for 文に for in 構文を追加

## 24/5

- match 文の条件節において`,`を使ったものを特別扱いする構文を追加

## 25/?

- match 文で全 branch を`.. => ..`の形式にしたときに trial match(上から順に試して最初に成功したのを採用する) として解釈するようにした

## 25/9-10

- 符号付き整数型を sN から iN に変更した。
