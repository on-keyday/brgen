
# generator specification file
#####
##### draft version 1
##### lang-version: 0.0.0.d1.a # draft 1
##### document version: 0.0.1
##### author: on-keyday (https://github.com/on-keyday)
##### note: this is implementation guidelines for my first parser and generator

## 1. Introduction
this specification defines how to tell generator specification to brgen, 
a generator driver

## 2. Format
generator specification file is as JSON file

## 3. Elements
```yaml
pass_by: | 
    tell brgen how to pass parsed AST to generator. 
    selection are stdin,file
    if file, file is passed by command line
    default is stdin.
langs: |
    language is programming language to generate by generator
    this MUST be array of string
types: |
    type configuration of brgen
    this is passed to src2json for typing rule 
```

## 4. Interface
brgen asks spec of generator with `<command name> -s`
generator should write spec to stdout

# History
2023/09/20: first version
2023/09/21: add types for Elements
