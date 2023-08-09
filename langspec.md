# brgen lang spec

# 変数定義
```py
x = 10 # 定義/代入(異型再代入可)
```
```py
x := 10 # 定義(同型再代入可/異型再代入不可)
```
```py
x ::= 10 # 定義 (定数/再代入不可)
```

# フォーマット定義
```py
name :String
if name == "hello": # 変数として使える
    print("world")
```

# IO
```py
input # 入力(キーワード)
output # 出力(キーワード)
```
