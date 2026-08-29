"""生成テキストの受け皿と、書き出し。"""

import io
import os


def write_if_changed(path: str, text: str) -> bool:
    """中身が変わったときだけ書く。書いたら True。

    無条件に書くと mtime が動き、ビルドはそれを見て後続を全部やり直す。
    nodes.h は全 TU が読むので、内容が同じでも 20 ファイル再コンパイルに
    なっていた。生成器を回すこと自体は速いので、比較して黙るほうがよい。
    """
    if os.path.exists(path):
        with open(path, encoding="utf-8", newline="") as f:
            if f.read() == text:
                return False
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    return True


class Writer:
    """複数の断片を順に受け取って 1 本のテキストにする。

    emit_* は出力先の Writer を引数で受け取る。グローバルに持たないのは、
    出力の順番を組み立てるのを呼び出し側 (nodegen.py) の責務にしておくため。
    """

    def __init__(self):
        self._buf = io.StringIO()

    def write(self, *args: str) -> None:
        for a in args:
            self._buf.write(a)

    def line(self, *args: str) -> None:
        self.write(*args)
        self.write("\n")

    def getvalue(self) -> str:
        return self._buf.getvalue()

    def __bool__(self) -> bool:
        return bool(self._buf.getvalue())
