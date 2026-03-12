# EBM Magic Access Path API Reference

This document outlines the valid "Magic Access Paths" that can be used to traverse the EBM (Extended Binary Module) data structure. These paths provide a convenient way to access nested data by automatically handling reference dereferencing and fallbacks.

## Access Rules

The access logic adheres to the following rules:

1.  **Implicit Dereference**: `StatementRef`, `TypeRef`, and `ExpressionRef` are automatically dereferenced to their corresponding instances (`Statement`, `Type`, `Expression`). Other `Ref` types like `IdentifierRef` are not.
2.  **Name Resolution Fallback**: If a field is not found on a root type (`Statement`, `Type`, `Expression`), the system automatically attempts to find it within the `body` member of that type. For example, `Statement.kind` is resolved as `Statement.body.kind`.
3.  **Variant/Union Traversal**: The combination of the above rules allows direct-like access to fields within variant types (e.g., accessing `var_decl` fields directly from a `Statement` object).
4.  **Instance Access (`.instance`)**: The special `.instance` keyword can be used to retrieve the object that a reference points to, rather than accessing one of its members. This is useful for getting the dereferenced object itself.

## Access Path Reference

### Instance Access (`.instance`)

The `.instance` keyword provides a way to get the underlying object from a reference.

| Root Type    | Access Path Example            | Resolved Type      | Logic Used                           | Description                                                              |
| :----------- | :----------------------------- | :----------------- | :----------------------------------- | :----------------------------------------------------------------------- |
| `Statement`  | `id.instance`                  | `Statement*`       | Ref(Stmt) + Instance                 | After `id` (`StatementRef`) is dereferenced, `.instance` returns the `Statement` object itself. |
| `Expression` | `type.instance`                | `Type*`            | Fallback + Ref(Type) + Instance      | After `type` (`TypeRef`) is dereferenced, `.instance` returns the `Type` object itself. |
| `Type`       | `body.id.instance`             | `Statement*`       | Direct + Ref(Stmt) + Instance        | For a `STRUCT` or `ENUM` type, accesses `body.id` (`StatementRef`), dereferences it, and `.instance` returns the `Statement` object itself. |

### Root Type: `Statement`

| Root Type    | Access Path Example            | Resolved Type   | Logic Used                  | Description                                                              |
| :----------- | :----------------------------- | :-------------- | :-------------------------- | :----------------------------------------------------------------------- |
| `Statement`  | `id`                           | `StatementRef`  | Direct                      | Accesses the unique ID of the statement.                                 |
| `Statement`  | `body`                         | `StatementBody` | Direct                      | Accesses the main body union of the statement.                           |
| `Statement`  | `kind`                         | `StatementKind` | Fallback                    | Accesses `body.kind` to get the specific statement type.                 |
| `Statement`  | `var_decl`                     | `VariableDecl`  | Fallback                    | Accesses `body.var_decl` for variable declarations.                      |
| `Statement`  | `var_decl.name`                | `IdentifierRef` | Fallback                    | Accesses the name of the declared variable. Not dereferenced.            |
| `Statement`  | `var_decl.var_type`            | `TypeRef`       | Fallback                    | Accesses the type reference of the declared variable.                    |
| `Statement`  | `struct_decl.fields`           | `Block`         | Fallback                    | Accesses the block of fields in a struct declaration.                    |
| `Statement`  | `struct_decl.fields.0`         | `StatementRef`  | Fallback + Index            | Accesses the first field statement (`FieldDecl`) in a struct.            |
| `Statement`  | `func_decl.return_type`        | `TypeRef`       | Fallback                    | Accesses the return type of a function declaration.                      |
| `Statement`  | `func_decl.return_type.kind`   | `TypeKind`      | Fallback + Ref(Type)        | Accesses the kind of the return type (`Type.body.kind`).                 |
| `Statement`  | `if_statement.then_block`      | `StatementRef`  | Fallback                    | Accesses the 'then' block of an if statement.                            |
| `Statement`  | `if_statement.then_block.kind` | `StatementKind` | Fallback + Ref(Stmt)        | Accesses the kind of the statement within the 'then' block.              |

### Root Type: `Expression`

| Root Type    | Access Path Example         | Resolved Type    | Logic Used                   | Description                                                              |
| :----------- | :-------------------------- | :--------------- | :--------------------------- | :----------------------------------------------------------------------- |
| `Expression` | `id`                        | `ExpressionRef`  | Direct                       | Accesses the unique ID of the expression.                                |
| `Expression` | `type`                      | `TypeRef`        | Fallback                     | Accesses `body.type`, the type reference of the expression result.       |
| `Expression` | `type.kind`                 | `TypeKind`       | Fallback + Ref(Type)         | Accesses the kind of the expression's type (`Type.body.kind`).           |
| `Expression` | `kind`                      | `ExpressionKind` | Fallback                     | Accesses `body.kind` to get the specific expression type.                |
| `Expression` | `bop`                       | `BinaryOp`       | Fallback                     | For `BINARY_OP`, accesses `body.bop` to get the operator.                |
| `Expression` | `bop.left`                  | `ExpressionRef`  | Fallback                     | For `BINARY_OP`, accesses `body.left`, the left-hand operand.            |
| `Expression` | `bop.left.kind`             | `ExpressionKind` | Fallback + Ref(Expr)         | Accesses the kind of the left-hand operand expression.                   |
| `Expression` | `identifier`                | `StatementRef`   | Fallback                     | For `IDENTIFIER` expression, accesses `body.id` which is a `StatementRef` to the declaration. |
| `Expression` | `identifier.kind`           | `StatementKind`  | Fallback + Ref(Stmt)         | Accesses the kind of the statement that declared the identifier.         |
| `Expression` | `identifier.var_decl.name`  | `IdentifierRef`  | Fallback + Ref(Stmt)         | Accesses the name of the variable declaration for this identifier.       |
| `Expression` | `call_desc.callee`          | `ExpressionRef`  | Fallback                     | Accesses the callee expression of a function call.                       |
| `Expression` | `call_desc.arguments.0`     | `ExpressionRef`  | Fallback + Index             | Accesses the first argument of a function call.                          |

### Root Type: `Type`

| Root Type | Access Path Example            | Resolved Type   | Logic Used                  | Description                                                              |
| :-------- | :----------------------------- | :-------------- | :-------------------------- | :----------------------------------------------------------------------- |
| `Type`    | `id`                           | `TypeRef`       | Direct                      | Accesses the unique ID of the type.                                      |
| `Type`    | `body`                         | `TypeBody`      | Direct                      | Accesses the main body union of the type.                                |
| `Type`    | `kind`                         | `TypeKind`      | Fallback                    | Accesses `body.kind` to get the specific type kind.                      |
| `Type`    | `size`                         | `Varint`        | Fallback                    | For `INT` or `UINT`, accesses `body.size` to get the size in bits.       |
| `Type`    | `element_type`                 | `TypeRef`       | Fallback                    | For `ARRAY` or `VECTOR`, accesses `body.element_type`.                   |
| `Type`    | `element_type.kind`            | `TypeKind`      | Fallback + Ref(Type)        | Accesses the kind of the array/vector element type.                      |
| `Type`    | `body.id`                      | `StatementRef`  | Direct                      | For `STRUCT` or `ENUM` types, accesses the `StatementRef` to the declaration. |
| `Type`    | `body.id.kind`                 | `StatementKind` | Direct + Ref(Stmt)          | Accesses the kind of the referenced declaration (e.g., `STRUCT_DECL`).   |
| `Type`    | `body.id.struct_decl.fields`   | `Block`         | Direct + Ref(Stmt)          | Accesses the fields of the referenced struct declaration.                |
| `Type`    | `variant_desc.members`         | `Types`         | Fallback                    | Accesses the list of member types in a `VARIANT` type.                   |
| `Type`    | `variant_desc.members.0`       | `TypeRef`       | Fallback + Index            | Accesses the first member type of a `VARIANT`.                           |
