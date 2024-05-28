---
title: "Metadata"
weight: 1
# bookFlatSection: false
# bookToc: true
# bookHidden: false
# bookCollapseSection: false
# bookComments: false
# bookSearchExclude: false
---

# メタデータ

ジェネレーター共通のメタデータについて述べる。

## 共通

### 元の URL のメタデータ

```
config.url = "https://example.com"
```

### RFC を指定する。

```
config.rfc = "https://example.com/url/to/rfc"
```

`config.url`を推奨する

## 言語固有

### C++

#### 名前空間名を指定

```
config.cpp.namespace = "namespace name"
```

名前空間名の整合性については保証しない

### Go

#### パッケージ名を指定

```
config.go.package = "package name"
```
