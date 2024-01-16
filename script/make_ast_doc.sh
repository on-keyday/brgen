#!/bin/bash


ER_DIAGRAM=`./tool/gen_ast2mermaid stdout`
FLOW_CHART=`./tool/gen_ast2mermaid -flow stdout | grep derive`
JSON=`./tool/src2json --dump-types | jq`

#echo "$ER_DIAGRAM"
#echo "$FLOW_CHART"
#echo "$JSON"

#return

DOC=$(cat <<EOF
---
title: "Ast"
weight: 1
# bookFlatSection: false
# bookToc: true
# bookHidden: false
# bookCollapseSection: false
# bookComments: false
# bookSearchExclude: false
---

# AST
###### generated by script/make_ast_doc.sh

brgen(lang) の AST

基本の継承関係

\`\`\`
Node
|        |      |
Expr     Stmt   Type
|        |
Literal  Member
\`\`\`

継承関係のみ(zoom 可)
\`\`\`mermaid
flowchart LR
${FLOW_CHART}
\`\`\`

継承関係+保持関係(zoom 可)
TODO(on-keyday): ぐちゃぐちゃすぎるのでどうにかしたい

\`\`\`mermaid
${ER_DIAGRAM}
\`\`\`

元ソース

\`\`\`
${ER_DIAGRAM}
\`\`\`

JSON 形式(上記の元データ)

TODO(on-keyday): 各ノードの説明文を入れる

\`\`\`json
${JSON}
\`\`\`

{{< mermaid >}}

EOF
)


echo "$DOC" > ./web/doc/content/docs/ast.md
