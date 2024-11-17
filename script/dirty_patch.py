TARGET = "src/tool/json2rust/pkg/json2rust.d.ts"
with open(TARGET, "r") as f:
    text = f.read()

text = text.replace("=> Array", "=> number[]")

with open(TARGET, "w") as f:
    f.write(text)
