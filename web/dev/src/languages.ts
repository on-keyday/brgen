import { Language, LanguageList } from "./s2j/msg";
import { BM_LANGUAGES, BM_LSP_LANGUAGES } from "./lib/bmgen/bm_caller.js";
import { EBM_LANGUAGES, EBM_LSP_LANGUAGES } from "./lib/bmgen/ebm_caller.js";

export const enum LanguageCategory {
    ANALYSIS = "Analysis",
    GENERATOR = "Direct Generator",
    INTERMEDIATE = "Intermediate",
    BM = "Binary Module",
    EBM = "Extended Binary Module",
}

export interface LanguageMeta {
    /** The language identifier used throughout the system (matches Language/RequestLanguage enum values) */
    id: string;
    /** Human-readable display name for the UI */
    displayName: string;
    /** Monaco editor language ID for syntax highlighting */
    monacoLang: string;
    /** Grouping category for the language selector */
    category: LanguageCategory;
}

/**
 * Monaco language mappings for core languages.
 * BM/EBM languages use BM_LSP_LANGUAGES / EBM_LSP_LANGUAGES instead.
 */
const coreMonacoLang: Record<string, string> = {
    [Language.TOKENIZE]: "json",
    [Language.JSON_AST]: "json",
    [Language.JSON_DEBUG_AST]: "json",
    [Language.CPP]: "cpp",
    [Language.GO]: "go",
    [Language.C]: "c",
    [Language.RUST]: "rust",
    [Language.TYPESCRIPT]: "typescript",
    [Language.KAITAI_STRUCT]: "yaml",
    [Language.BINARY_MODULE]: "plaintext",
    [Language.EBM]: "plaintext",
};

const coreDisplayName: Record<string, string> = {
    [Language.TOKENIZE]: "Tokenize",
    [Language.JSON_AST]: "JSON AST",
    [Language.JSON_DEBUG_AST]: "JSON AST (debug)",
    [Language.CPP]: "C++",
    [Language.GO]: "Go",
    [Language.C]: "C",
    [Language.RUST]: "Rust",
    [Language.TYPESCRIPT]: "TypeScript",
    [Language.KAITAI_STRUCT]: "Kaitai Struct",
    [Language.BINARY_MODULE]: "Binary Module",
    [Language.EBM]: "Extended Binary Module",
};

const coreCategory: Record<string, LanguageCategory> = {
    [Language.TOKENIZE]: LanguageCategory.ANALYSIS,
    [Language.JSON_AST]: LanguageCategory.ANALYSIS,
    [Language.JSON_DEBUG_AST]: LanguageCategory.ANALYSIS,
    [Language.CPP]: LanguageCategory.GENERATOR,
    [Language.GO]: LanguageCategory.GENERATOR,
    [Language.C]: LanguageCategory.GENERATOR,
    [Language.RUST]: LanguageCategory.GENERATOR,
    [Language.TYPESCRIPT]: LanguageCategory.GENERATOR,
    [Language.KAITAI_STRUCT]: LanguageCategory.GENERATOR,
    [Language.BINARY_MODULE]: LanguageCategory.INTERMEDIATE,
    [Language.EBM]: LanguageCategory.INTERMEDIATE,
};

function buildLanguageRegistry(): LanguageMeta[] {
    const registry: LanguageMeta[] = [];

    // Core languages from LanguageList
    for (const lang of LanguageList) {
        registry.push({
            id: lang,
            displayName: coreDisplayName[lang] ?? lang,
            monacoLang: coreMonacoLang[lang] ?? "plaintext",
            category: coreCategory[lang] ?? LanguageCategory.GENERATOR,
        });
    }

    // BM-based languages (dynamically registered, may be empty if stubs are used)
    for (const lang of BM_LANGUAGES) {
        registry.push({
            id: lang,
            displayName: lang,
            monacoLang: (BM_LSP_LANGUAGES as Record<string, string>)[lang] ?? "plaintext",
            category: LanguageCategory.BM,
        });
    }

    // EBM-based languages
    for (const lang of EBM_LANGUAGES) {
        registry.push({
            id: lang,
            displayName: lang,
            monacoLang: (EBM_LSP_LANGUAGES as Record<string, string>)[lang] ?? "plaintext",
            category: LanguageCategory.EBM,
        });
    }

    return registry;
}

/** All available languages with metadata, built once at module load. */
export const languageRegistry: readonly LanguageMeta[] = Object.freeze(buildLanguageRegistry());

/** All language IDs as a flat list (replaces scattered LanguageList.concat(BM_LANGUAGES).concat(EBM_LANGUAGES)). */
export const allLanguageIds: readonly string[] = Object.freeze(languageRegistry.map(l => l.id));

/** Look up metadata by language ID. Returns undefined for unknown IDs. */
export function getLanguageMeta(id: string): LanguageMeta | undefined {
    return languageRegistry.find(l => l.id === id);
}

/** Get the Monaco editor language ID for a given language. Falls back to "plaintext". */
export function getMonacoLang(id: string): string {
    return getLanguageMeta(id)?.monacoLang ?? "plaintext";
}

/** Get languages grouped by category, for building grouped dropdowns. */
export function getLanguagesByCategory(): Map<LanguageCategory, LanguageMeta[]> {
    const map = new Map<LanguageCategory, LanguageMeta[]>();
    for (const lang of languageRegistry) {
        let list = map.get(lang.category);
        if (!list) {
            list = [];
            map.set(lang.category, list);
        }
        list.push(lang);
    }
    return map;
}
