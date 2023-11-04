import ast
import subprocess as sp
import json

if __name__ == "__main__":
    # execute tool/src2json(.exe) example/tree_test.bgn and get stdout
    # (stdout is json format)
    cmd = ["./tool/src2json.exe", "example/tree_test.bgn"]
    proc = sp.Popen(cmd, stdout=sp.PIPE)
    stdout, stderr = proc.communicate()
    # create AstFile object
    d = json.loads(stdout.decode("utf-8"))
    file = ast.parse_AstFile(d)
    node = ast.ast2node(file.ast)
    assert isinstance(node, ast.Program)

    def walker(f, node):
        ast.walk(node, f)
        print(type(node))

    ast.walk(node, walker)
