/**
 * Single source of truth for language-specific code generation option definitions.
 *
 * Keys must match the string values of ConfigKey in types.ts.
 * Both the config store (browser) and language registry (server+browser) derive from this.
 *
 * BM/EBM options are not listed here; they are provided dynamically by
 * setBMUIConfig / setEBMUIConfig from the auto-generated bm_caller.js / ebm_caller.js.
 */

export interface OptionDef {
    /** Key used in the options record passed to generate() — matches ConfigKey values */
    key: string;
    type: "checkbox" | "number" | "text" | "choice";
    defaultValue: boolean | number | string;
    help?: string;
    candidates?: string[];
}

export const coreOptionDefs: Readonly<Record<string, readonly OptionDef[]>> = Object.freeze({
    "cpp": [
        { key: "source_map",          type: "checkbox", defaultValue: false },
        { key: "expand_include",      type: "checkbox", defaultValue: false },
        { key: "use_error",           type: "checkbox", defaultValue: false },
        { key: "use_raw_union",       type: "checkbox", defaultValue: false },
        { key: "check_overflow",      type: "checkbox", defaultValue: false },
        { key: "enum_stringer",       type: "checkbox", defaultValue: false },
        { key: "use_constexpr",       type: "checkbox", defaultValue: false },
        { key: "add_visit",           type: "checkbox", defaultValue: false },
        { key: "force_optional_getter", type: "checkbox", defaultValue: false },
    ],
    "go": [
        { key: "omit_must_encode",  type: "checkbox", defaultValue: false },
        { key: "omit_decode_exact", type: "checkbox", defaultValue: false },
        { key: "omit_visitors",     type: "checkbox", defaultValue: false },
        { key: "omit_marshal_json", type: "checkbox", defaultValue: false },
    ],
    "c": [
        { key: "multi_file",  type: "checkbox", defaultValue: false },
        { key: "omit_error",  type: "checkbox", defaultValue: false },
        { key: "use_memcpy",  type: "checkbox", defaultValue: false },
        { key: "zero_copy",   type: "checkbox", defaultValue: false },
    ],
    "typescript": [
        { key: "javascript", type: "checkbox", defaultValue: false },
    ],
    "binary module": [
        { key: "print_instruction", type: "checkbox", defaultValue: false },
    ],
    "ebm": [
        { key: "no_output",          type: "checkbox", defaultValue: false },
        { key: "print_instruction",  type: "checkbox", defaultValue: false },
        { key: "control_flow_graph", type: "checkbox", defaultValue: false },
    ],
});
