"""nast の nodes.h を生成する。

nodes.json (schema) が正本で、C++ ヘッダはその射影。
段ごとにモジュールを分けてあり、出力の順番を決めるのは nodegen.py の責務。

  schema.py   nodes.json の読み込みと正規化 (継承ビット、フィールド列、型解決)
  enums.py    enum / enum_array 相当のヘルパ / 演算子の優先順位層
  nodes.py    NodeType / NodeHeader / Node / RefBase / NodeData / 走査の入口
  arena.py    Arena (型別プール、make、get、for_each_pool)
  tables.py   素の値型と side table

生成するのは「型ごとに書き分けが要るもの」だけに寄せてある。
for_each_field と visit_node_type さえ出ていれば、その上の走査
(printer.h / from_json.h) は手書きの汎用コードで足りる。
"""

from .schema import Schema
from .writer import Writer, write_if_changed

__all__ = ["Schema", "Writer", "write_if_changed"]
