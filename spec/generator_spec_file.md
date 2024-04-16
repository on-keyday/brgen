# generator specification file

#####

##### draft version 1

##### lang-version: 0.0.0.d3 # draft 3

##### document version: 0.0.6

##### author: on-keyday (https://github.com/on-keyday)

##### note: this is implementation guidelines for my first parser and generator

## 1. Introduction

this specification defines how to tell generator specification to brgen, a generator driver

## 2. Format

generator specification file is a JSON file

## 3. Elements

```yaml
input: |
  tell brgen how to pass parsed AST to generator.  (string)
  selection are "stdin","file","stdin_stream"
  if file, file name is passed by command line
  if stdin_stream, 
  using binary based async protocol described in example/brgen_help/generator.bgn. 
  when using stdin_stream, other configurations are ignored
  default is stdin.
langs: |
  language is programming language to generate by generator (array of string)
suffix: |
  suffix of files (array of string)
  array of suffix like ['.cpp'] (not like '*.cpp')
  count of suffix is maximum count of generated file
separator: |
  separator of file (string)
  if element count of suffix is grater than 1, separator must be specified
  generator should use separator that is not a part of generated language to
  prevent mis separation
```

## 4. Interface

brgen asks spec of generator with `<command name> -s`
generator should write spec to stdout

## 5. Example

in this example, this generator will generate python file mainly and
sometimes, generate debug information file as json

```python
import sys
file = sys.argv[1]

if file == '-s':
    print("""
{
    "input": "stdin",
    "langs": ["python","json"],
    "suffix": [".py",".json"],
    "separator": "$#$#$#$#$#$#$#$#\n"
}
    """)
    sys.exit(0)

# implement generator...
```

brgen must passes `-s` option as `sys.argv[1]`

# History

2023/09/20: first version
2023/09/21: add types for Elements
2023/12/26: add document of separator and suffix
2023/12/26: add Example section
2024/04/16: add stdin_stream option
