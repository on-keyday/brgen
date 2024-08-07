import subprocess as sp

ret = sp.call(
    ["./tool/gen_ast2go", "./astlib/ast2go/ast/ast.go"],
    executable="./tool/gen_ast2go.exe",
)
print(ret)
ret = sp.call(
    ["gofmt", "-w", "./astlib/ast2go/ast/ast.go"],
)
print(ret)
ret = sp.call(
    ["./tool/gen_ast2ts", "./astlib/ast2ts/src/ast.ts"],
    executable="./tool/gen_ast2ts.exe",
)
print(ret)
ret = sp.call(["tsc"], cwd="./astlib/ast2ts", shell=True)
print(ret)
if ret == 0:
    import shutil

    shutil.copy("./LICENSE", "./astlib/ast2ts/out/LICENSE")
ret = sp.call(
    ["./tool/gen_ast2rust", "./astlib/ast2rust/core/src/ast.rs"],
    executable="./tool/gen_ast2rust.exe",
)
print(ret)
ret = sp.call(
    ["./tool/gen_ast2py", "./astlib/ast2py/ast.py"],
    executable="./tool/gen_ast2py.exe",
)
print(ret)

ret = sp.call(
    ["./tool/gen_ast2c", "./astlib/ast2c/ast.h", "./astlib/ast2c/ast.c"],
    executable="./tool/gen_ast2c.exe",
)

print(ret)

ret = sp.call(
    ["./tool/gen_ast2csharp", "./astlib/ast2csharp/ast.cs"],
    executable="./tool/gen_ast2csharp.exe",
)

print(ret)

ret = sp.call(
    ["./tool/gen_ast2dart", "./astlib/ast2dart/lib/ast.dart"],
    executable="./tool/gen_ast2dart.exe",
)

print(ret)
