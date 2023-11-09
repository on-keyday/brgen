import subprocess as sp

ret = sp.call(["go", "generate", "./ast2go"])
print(ret)
ret = sp.call(
    ["./tool/gen_ast2ts", "./ast2ts/src/ast.ts"],
    executable="./tool/gen_ast2ts.exe",
)
print(ret)
ret = sp.call(["tsc"], cwd="./ast2ts", shell=True)
print(ret)
ret = sp.call(
    ["./tool/gen_ast2rust", "./ast2rust/src/ast2rust/ast.rs"],
    executable="./tool/gen_ast2rust.exe",
)
print(ret)
ret = sp.call(
    ["./tool/gen_ast2py", "./ast2py/ast2py.py"],
    executable="./tool/gen_ast2py.exe",
)
print(ret)

ret = sp.call(
    ["./tool/gen_ast2c", "./ast2c/ast.h", "./ast2c/ast.c"],
    executable="./tool/gen_ast2c.exe",
)

print(ret)
