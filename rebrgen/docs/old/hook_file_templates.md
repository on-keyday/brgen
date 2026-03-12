## Hook File Templates

The `bm2` code generator uses a set of template files, referred to as "Hook Files", to generate code for different languages. These templates allow for customization of the generated code, such as specifying language-specific syntax, data types, and code structures. Hook files are located in the `src/old/bm2{lang_name}/hook/` directories.

The `src/old/bm2/gen_template/` directory contains the core logic for reading and processing these hook files. The `src/old/bm2/gen_template.cpp` file is responsible for reading the template files and using them to generate code. The `may_write_from_hook` function reads the content of a template file and inserts it into the generated code.

The `src/old/bm2/gen_template/hook_list.bgn` file defines an enum `HookFile` that lists all available template files.

### Template Categories

The Hook File templates are categorized into several groups:

- **General file structure:** These templates define the overall structure of the generated file.

  - `file_top.txt`: Contains the code that is placed at the beginning of the generated file, such as includes and namespace declarations.
  - `file_top_after.txt`: Contains code placed after the initial file setup.
  - `file_bottom.txt`: Contains the code that is placed at the end of the generated file.

- **Context definitions:** These templates define the context object that is used during code generation.

- **Command-line flag definitions:** These templates define the command-line flags that are used to configure the code generator.

  - `flags.txt`: Contains the definitions for the command-line flags.

- **Inner function/block related templates:** These templates define the structure of inner functions and blocks of code.

  - `inner_function_start.txt`: Contains the code that is placed at the beginning of an inner function.
  - `inner_function_each_code.txt`: Contains the code that is executed for each instruction within an inner function.
  - `inner_block_start.txt`: Contains the code that is placed at the beginning of an inner block.
  - `inner_block_each_code.txt`: Contains the code that is executed for each instruction within an inner block.

- **Parameter related templates:** These templates define how parameters are handled in the generated code.

  - `param_start.txt`: Contains the code that is placed at the beginning of a parameter list.
  - `param_each_code.txt`: Contains the code that is executed for each parameter in a parameter list.
  - `call_param_start.txt`: Contains the code that is placed at the beginning of a function call's parameter list.
  - `call_param_each_code.txt`: Contains the code that is executed for each parameter in a function call's parameter list.

- **Operation-specific templates:** These templates define the code that is generated for specific abstract operations.
  - `inner_function_op.txt`: Contains the code that is generated for a specific abstract operation within an inner function. The template file name is formatted as `func_{}.txt`, where `{}` is replaced with the lowercase name of the abstract operation.
  - `inner_block_op.txt`: Contains the code that is generated for a specific abstract operation within an inner block. The template file name is formatted as `block_{}.txt`, where `{}` is replaced with the lowercase name of the abstract operation.
  - `eval_op.txt`: Contains the code that is generated for evaluating a specific abstract operation. The template file name is formatted as `eval_{}.txt`, where `{}` is replaced with the lowercase name of the abstract operation.
  - `type_op.txt`: Contains the code that is generated for a specific storage type. The template file name is formatted as `type_{}.txt`, where `{}` is replaced with the lowercase name of the storage type.
  - `param_op.txt`: Contains the code that is generated for a specific abstract operation when defining parameters. The template file name is formatted as `param_{}.txt`, where `{}` is replaced with the lowercase name of the abstract operation.
  - `call_param_op.txt`: Contains the code that is generated for a specific abstract operation when calling functions with parameters. The template file name is formatted as `call_param_{}.txt`, where `{}` is replaced with the lowercase name of the abstract operation.

### Usage

The `gen_template.cpp` file reads the binary module and iterates through its code, using the `AbstractOp` enum to determine the type of instruction. For each instruction, it calls the `may_write_from_hook` function with the appropriate `HookFile` enum value to insert the corresponding template code into the generated code.

The template files can contain placeholders that are replaced with language-specific values. These placeholders are defined and managed within the `src/old/bm2/gen_template/` directory.

### Parameters and Variables

The `docs/template_parameters.md` file describes the variables that can be used in the code generator-generator hooks. These include:

- **Words:**

  - `reference`: A reference to a several object in the binary module.
  - `type reference`: A reference to a type in the binary module.
  - `identifier`: An identifier of a several object (e.g. function, variable, types, etc.)
  - `EvalResult`: result of eval() function. it contains the result of the expression evaluation.
  - `placeholder`: in some case, code generator placeholder in `config.json` can be replaced with context specific value by envrionment variable like format.

- **Function `eval`:**

  - Variables:
    - `left_eval_ref`: reference of left operand
    - `left_eval`: left operand
    - `right_ident_ref`: reference of right operand
    - `right_ident`: identifier of right operand
  - Placeholder Mappings:
    - `eval_result_text`: RESULT, TEXT

- **Function `inner_function`:**

  - Variables:
    - ... (See `docs/template_parameters.md` for a complete list)

- **Function `inner_block`:**

  - Variables:
    - ... (See `docs/template_parameters.md` for a complete list)

- **Function `add_parameter`:**

  - Variables:
    - ... (See `docs/template_parameters.md` for a complete list)

- **Function `add_call_parameter`:**

  - Variables:
    - ... (See `docs/template_parameters.md` for a complete list)

- **Function `type_to_string`:**

  - Variables:
    - ... (See `docs/template_parameters.md` for a complete list)

- **Function `field_accessor`:**

  - Variables:
    - ... (See `docs/template_parameters.md` for a complete list)

- **Function `type_accessor`:**
  - Variables:
    - ... (See `docs/template_parameters.md` for a complete list)

### Flags

The `Flags` struct in `src/old/bm2/gen_template/flags.hpp` defines variables that can be used as placeholders in the hook files. These flags provide language-specific information and settings. Some examples include:

- `lang_name`: The name of the target language.
- `comment_prefix`: The prefix for comments.
- `comment_suffix`: The suffix for comments.
- `int_type_placeholder`: The placeholder for integer types (e.g., `std::int{}_t`).
- `uint_type_placeholder`: The placeholder for unsigned integer types (e.g., `std::uint{}_t`).
- `float_type_placeholder`: The placeholder for floating-point types (e.g., `float{}_t`).
- `array_type_placeholder`: The placeholder for array types (e.g., `std::array<{}, {}>`).
- `array_has_one_placeholder`: A boolean indicating whether the array type has one placeholder.
- `vector_type_placeholder`: The placeholder for vector types (e.g., `std::vector<{}>`).
- `optional_type_placeholder`: The placeholder for optional types (e.g., `std::optional<{}>`).
- `pointer_type_placeholder`: The placeholder for pointer types (e.g., `{}*`).
- `bool_type`: The string representation of the boolean type (e.g., `bool`).
- `true_literal`: The string representation of the true literal (e.g., `true`).
- `false_literal`: The string representation of the false literal (e.g., `false`).
- `coder_return_type`: The return type for coder functions (e.g., `bool`).
- `property_setter_return_type`: The return type for property setter functions (e.g., `bool`).
- `end_of_statement`: The string that marks the end of a statement (e.g., `;`).
- `block_begin`: The string that marks the beginning of a block (e.g., `{`).
- `block_end`: The string that marks the end of a block (e.g., `}`).
- `block_end_type`: The string that marks the end of a type definition block (e.g., `};`).
- `struct_keyword`: The keyword for defining structs (e.g., `struct`).
- `enum_keyword`: The keyword for defining enums (e.g., `enum`).
- `define_var_keyword`: The keyword for defining variables.
- `var_type_separator`: The separator between a variable name and its type.
- `field_type_separator`: The separator between a field name and its type.
- `field_end`: The string that marks the end of a field definition (e.g., `;`).
- `enum_member_end`: The string that marks the end of an enum member definition (e.g., `,`).
- `func_keyword`: The keyword for defining functions.
- `func_type_separator`: The separator between a function name and its return type.
- `func_void_return_type`: The string representation of the void return type (e.g., `void`).
- `if_keyword`: The keyword for if statements.
- `elif_keyword`: The keyword for else if statements.
- `else_keyword`: The keyword for else statements.
- `infinity_loop`: The string representation of an infinite loop (e.g., `for(;;)`).
- `conditional_loop`: The keyword for conditional loops (e.g., `while`).
- `self_ident`: The identifier for the self object (e.g., `(*this)`).
- `param_type_separator`: The separator between a parameter name and its type.
- `self_param`: The string representation of the self parameter.
- `encoder_param`: The parameters for encoder functions (e.g., `Encoder& w`).
- `decoder_param`: The parameters for decoder functions (e.g., `Decoder& w`).
- `func_style_cast`: Whether to use function-style casts.

See also `tool/gen_template --mode config` for full list of flags name and
