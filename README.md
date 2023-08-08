# brgen - binary representation interpreter generator
ネットワーク・プロトコルのパケット等の解析/生成のためのコードを吐くジェネレータ

[x]: lexer 
[x]: ast


# 目標(Goal)
+ lightweight runtime - ランタイムは軽いもしくは無い
+ enough to represent formats - 世の中にあるネットワークプロトコルフォーマットを表現するのに十分な表現力
+ easy to write - 簡単に書ける
+ write once generate any language code - 一回書けば様々な言語で生成
