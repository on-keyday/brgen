import subprocess as sp

ret = sp.call(["go", "generate", "./ast2go"])
print(ret)
ret = sp.call(
    ["./build/tool/gen_ast2ts", "./ast2ts/src/ast2ts.ts"],
    executable="./build/tool/gen_ast2ts.exe",
)
print(ret)
ret = sp.call(
    ["./build/tool/gen_ast2rust", "./ast2rust/src/ast2rust/ast.rs"],
    executable="./build/tool/gen_ast2rust.exe",
)
print(ret)
