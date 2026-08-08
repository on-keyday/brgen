"""生成テキストの受け皿。"""

import io


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
