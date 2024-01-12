import ast2py.ast as ast


class EvalExpr:
    identMap: dict[ast.Ident, str]

    def lookupBase(self, ident: ast.Ident) -> tuple(ast.Ident, bool):
        viaMember = False
        while ident.base is not None:
            if isinstance(ident.base, ast.Ident):
                ident = ident.base
                continue
            if isinstance(ident.base, ast.MemberAccess):
                assert isinstance(ident.base.base, ast.Ident)
                ident = ident.base.base
                viaMember = True
                continue
            break
        return tuple(ident, viaMember)

    def registerIdent(self, ident: ast.Ident, name: str):
        ident, _ = self.lookupBase(ident)
        self.identMap[ident] = name

    def ident_to_string(self, ident: ast.Ident):
        b, viaMember = self.lookupBase(ident)
        if not viaMember:
            if isinstance(b.base, ast.Member):
                return f"self.{self.identMap[b]}"
        return self.identMap[b]

    def to_string(self, e: ast.Expr):
        if isinstance(e, ast.Ident):
            return self.ident_to_string(e)
        elif isinstance(e, ast.MemberAccess):
            return f"{self.to_string(e.base)}.{self.to_string(e.member)}"
        elif isinstance(e, ast.IntLiteral):
            return e.value
        elif isinstance(e, ast.StringLiteral):


class Generator:
    w: bytes
    indent: int
    seq: int
    struct_map: dict[ast.StructType, str]
    evalExpr: EvalExpr

    def __init__(self) -> None:
        self.w = b""
        self.indent = 0
        self.seq = 0
        self.struct_map = {}
        self.evalExpr = EvalExpr()

    def writeIndent(self):
        self.w += b"    " * self.indent

    def write(self, s: str):
        self.w += s.encode("utf-8")

    def getSeq(self):
        s = self.seq
        self.seq += 1
        return s

    def generate_root(self, prog: ast.Program):
        for element in prog.elements:
            if isinstance(element, ast.Format):
                self.generate_format(element)

    def generate_format(self, fmt: ast.Format):
        self.generate_definition(fmt.body.struct_type)
        if fmt.encode_fn is not None:
            self.generate_encoder(fmt.encode_fn.body)
        else:
            self.generate_encoder(fmt.body)

        if fmt.decode_fn is not None:
            self.generate_decoder(fmt.decode_fn.body)
        else:
            self.generate_decoder(fmt.body)

    def generate_definition(self, typ: ast.StructType, prefix: str = ""):
        if isinstance(typ.base, ast.Member):
            self.writeIndent()
            self.write(f"class {prefix}{typ.base.ident.ident}:\n")
            self.indent += 1
        for field in typ.fields:
            if isinstance(field, ast.Field):
                self.generate_field(field, prefix)

    def type_to_str(self, typ: ast.Type) -> str:
        if isinstance(typ, ast.IdentType):
            return typ.ident.ident
        elif isinstance(typ, ast.ArrayType):
            return f"List[{self.type_to_str(typ.base_type)}]"
        elif isinstance(typ, ast.IntType):
            return "int"

    def generate_struct_union(self, typ: ast.StructUnionType, prefix: str = ""):
        for s in typ.structs:
            new_prefix = f"{prefix}union_{self.getSeq()}_"
            self.struct_map[s] = new_prefix
            self.generate_definition(s, new_prefix)

    def generate_union(self, field: ast.Field, prefix: str = ""):
        self.writeIndent()
        self.write("def get_", field.ident.ident, "(self):\n")
        self.indent += 1
        assert isinstance(field.field_type, ast.UnionType)
        cond0 = None if cond0 is None else self.evalExpr.to_string(cond0)
        for t in field.field_type.candidates:
            self.writeIndent()
            if cond0 is not None:
                self.write(f"if self.{cond0} == {t.ident.ident}:\n")

    def generate_field(self, field: ast.Field, prefix: str = ""):
        # special cases; union
        if isinstance(field.field_type, ast.StructUnionType):
            self.generate_struct_union(field.field_type, prefix)
            return
        if isinstance(field.field_type, ast.UnionType):
            self.generate_union(field.field_type, prefix)
            return
        self.writeIndent()
        self.write(f"{prefix}{field.ident.ident}: {self.type_to_str(prefix)}\n")


if __name__ == "__main__":
    import json
    import sys

    a = ast.parse_AstFile(json.load(sys.stdin.buffer.read()))
    Ast = ast.ast2node(a.ast)
    g = Generator()
    g.generate_root(Ast)

    print(g.w.decode("utf-8"))
